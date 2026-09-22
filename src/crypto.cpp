// ------------------------------------------------------------------
// Nstrike — secp256k1 en coordonnées planes (mulMod/invMod), ECDSA
// déterministe RFC 6979, récupération de clé (ecrecover), adresses.
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#include "crypto.hpp"

#include <cstring>

#include "keccak.hpp"
#include "sha256.hpp"

namespace nstrike {

// ---------------------------------------------------------------
// Paramètres secp256k1
// ---------------------------------------------------------------
static const u256 P_ = u256::fromHex("fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f");
static const u256 N_ = u256::fromHex("fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141");
static const u256 GX_ = u256::fromHex("79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798");
static const u256 GY_ = u256::fromHex("483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8");

const u256& secp256k1P() { return P_; }
const u256& secp256k1N() { return N_; }
const u256& secp256k1Gx() { return GX_; }
const u256& secp256k1Gy() { return GY_; }


// ---------------------------------------------------------------
// Arithmétique de champ réduite sur 257 bits effectifs.
// addMod/subMod de u256 ignorent le report au-delà de 2^256 : des
// chaînes comme 2D ou 3A (ou r+p quand r≥p) perdraient le bit haut.
// Ici on maintient explicitement un haut-mot `high` (en multiple de
// 2^256) et on replie modulo `mod` par soustractions/aditions.
// ---------------------------------------------------------------
static void fold(u256& v, int& high, const u256& mod) {
    // phase positive : v + high*2^256 ≥ mod → soustraire mod
    while (high > 0 || (high == 0 && v >= mod)) {
        uint64_t borrow = 0;
        for (int i = 0; i < 4; ++i) {
            __uint128_t bb = (__uint128_t)mod.l[i] + borrow;
            uint64_t d = (uint64_t)((__uint128_t)v.l[i] - bb);
            borrow = ((__uint128_t)v.l[i] < bb) ? 1 : 0;
            v.l[i] = d;
        }
        if (borrow) --high; // emprunt depuis le haut-mot
    }
    // phase négative : valeur < 0 → ajouter mod jusqu'à redevenir ≥ 0
    while (high < 0) {
        uint64_t c = 0;
        for (int i = 0; i < 4; ++i) {
            __uint128_t s = (__uint128_t)v.l[i] + mod.l[i] + c;
            v.l[i] = (uint64_t)s;
            c = (uint64_t)(s >> 64);
        }
        high += (int)c;
    }
    // après ajouts, v est enfin < mod (voir preuve : sortie via report)
    while (high > 0 || (high == 0 && v >= mod)) {
        uint64_t borrow = 0;
        for (int i = 0; i < 4; ++i) {
            __uint128_t bb = (__uint128_t)mod.l[i] + borrow;
            uint64_t d = (uint64_t)((__uint128_t)v.l[i] - bb);
            borrow = ((__uint128_t)v.l[i] < bb) ? 1 : 0;
            v.l[i] = d;
        }
        if (borrow) --high;
    }
}

static void fAdd(u256& r, const u256& a, const u256& mod) {
    int high = 0;
    uint64_t c = 0;
    for (int i = 0; i < 4; ++i) {
        __uint128_t s = (__uint128_t)r.l[i] + a.l[i] + c;
        r.l[i] = (uint64_t)s;
        c = (uint64_t)(s >> 64);
    }
    high = (int)c;
    fold(r, high, mod);
}

static void fSub(u256& r, const u256& a, const u256& mod) {
    uint64_t borrow = 0;
    u256 d;
    for (int i = 0; i < 4; ++i) {
        __uint128_t bb = (__uint128_t)a.l[i] + borrow;
        uint64_t dv = (uint64_t)((__uint128_t)r.l[i] - bb);
        borrow = ((__uint128_t)r.l[i] < bb) ? 1 : 0;
        d.l[i] = dv;
    }
    int high = -((int)borrow);
    fold(d, high, mod);
    r = d;
}

// ---------------------------------------------------------------
// Arithmétique Jacobienne (a = 0), coordonnées en arithmétique modulaire
// classique (mulMod/invMod) : plus simple à valider que les cosets de
// Montgomery, qui exigeaient une inversion "coset" pour l'affine.
// ---------------------------------------------------------------
struct JPoint {
    u256 X = u256::one();
    u256 Y = u256::one();
    u256 Z;              // Z = 0 => point à l'infini
};

static void jDouble(JPoint& Q) {
    if (Q.Z.isZero()) return;
    const u256& p = P_;
    u256 A = u256::mulMod(Q.X, Q.X, p);
    u256 B = u256::mulMod(Q.Y, Q.Y, p);
    u256 C = u256::mulMod(B, B, p);
    // D = 2*((X+B)^2 - A - C)
    u256 XpB = Q.X; fAdd(XpB, B, p);
    u256 D = u256::mulMod(XpB, XpB, p);
    fSub(D, A, p); fSub(D, C, p);
    fAdd(D, D, p);
    u256 E = A; fAdd(E, A, p); fAdd(E, A, p);
    u256 G = u256::mulMod(E, E, p);
    u256 X3 = G; fSub(X3, D, p); fSub(X3, D, p);
    u256 Y3 = D; fSub(Y3, X3, p);          // D - X3
    Y3 = u256::mulMod(Y3, E, p);           // E*(D - X3)
    u256 C8 = C; for (int t = 0; t < 7; ++t) fAdd(C8, C, p); // 8*C mod p (un décalage déborderait 2^256)
    fSub(Y3, C8, p);
    u256 Z3 = u256::mulMod(Q.Y, Q.Z, p);
    fAdd(Z3, Z3, p);
    Q.X = X3; Q.Y = Y3; Q.Z = Z3;
}

static void jAdd(JPoint& R, const JPoint& P1, const JPoint& P2) {
    if (P1.Z.isZero()) { R = P2; return; }
    if (P2.Z.isZero()) { R = P1; return; }
    const u256& p = P_;
    u256 Z1Z1 = u256::mulMod(P1.Z, P1.Z, p);
    u256 Z2Z2 = u256::mulMod(P2.Z, P2.Z, p);
    u256 U1 = u256::mulMod(P1.X, Z2Z2, p);
    u256 U2 = u256::mulMod(P2.X, Z1Z1, p);
    u256 S1 = u256::mulMod(P1.Y, u256::mulMod(Z2Z2, P2.Z, p), p);
    u256 S2 = u256::mulMod(P2.Y, u256::mulMod(Z1Z1, P1.Z, p), p);
    u256 H = U2; fSub(H, U1, p);
    u256 Rv = S2; fSub(Rv, S1, p);
    if (H.isZero()) {
        if (Rv.isZero()) { R = P1; jDouble(R); return; }
        R = JPoint{}; // infini
        return;
    }
    u256 H2 = u256::mulMod(H, H, p);
    u256 H3 = u256::mulMod(H, H2, p);
    u256 U1H2 = u256::mulMod(U1, H2, p);
    u256 X3 = u256::mulMod(Rv, Rv, p);
    fSub(X3, H3, p);
    fSub(X3, U1H2, p); fSub(X3, U1H2, p);
    u256 Y3 = U1H2; fSub(Y3, X3, p);
    Y3 = u256::mulMod(Y3, Rv, p);
    fSub(Y3, u256::mulMod(S1, H3, p), p);
    u256 Z3 = u256::mulMod(P1.Z, P2.Z, p);
    Z3 = u256::mulMod(Z3, H, p);
    R.X = X3; R.Y = Y3; R.Z = Z3;
}

static PublicKey jToAffine(const JPoint& Q) {
    if (Q.Z.isZero()) return PublicKey{};
    const u256& p = P_;
    u256 zInv = u256::invMod(Q.Z, p);
    u256 zInv2 = u256::mulMod(zInv, zInv, p);
    u256 zInv3 = u256::mulMod(zInv2, zInv, p);
    PublicKey out;
    out.x = u256::mulMod(Q.X, zInv2, p);
    out.y = u256::mulMod(Q.Y, zInv3, p);
    return out;
}

static JPoint jFromAffine(const PublicKey& Q) {
    if (Q.x.isZero() && Q.y.isZero()) return JPoint{};
    JPoint out;
    out.X = Q.x;
    out.Y = Q.y;
    out.Z = u256::one();
    return out;
}

static JPoint jScalarMul(const u256& k, const JPoint& base) {
    JPoint result{}; // infini
    JPoint addend = base;
    for (int i = 255; i >= 0; --i) {
        jDouble(result);
        if (k.bit(i)) jAdd(result, result, addend);
    }
    return result;
}

PublicKey ecMulG(const u256& k) {
    JPoint G;
    G.X = GX_;
    G.Y = GY_;
    G.Z = u256::one();
    return jToAffine(jScalarMul(k, G));
}

PublicKey ecMul(const u256& k, const PublicKey& Q) {
    return jToAffine(jScalarMul(k, jFromAffine(Q)));
}

PublicKey ecAdd(const PublicKey& a, const PublicKey& b) {
    JPoint R;
    jAdd(R, jFromAffine(a), jFromAffine(b));
    return jToAffine(R);
}

bool ecIsInfinity(const PublicKey& p) { return p.x.isZero() && p.y.isZero(); }

// ---------------------------------------------------------------
// HMAC-SHA256
// ---------------------------------------------------------------
fix32 hmacSha256(const bytes& key, const bytes& msg) {
    bytes k;
    if (key.size() > 64) {
        fix32 h = SHA256::hash(key);
        k.assign(h.begin(), h.end());
    } else {
        k = key;
    }
    if (k.size() < 64) k.resize(64, 0);
    bytes ipad(64, 0x36), opad(64, 0x5c);
    for (size_t i = 0; i < 64; ++i) {
        ipad[i] ^= k[i];
        opad[i] ^= k[i];
    }
    bytes inner;
    inner.insert(inner.end(), ipad.begin(), ipad.end());
    inner.insert(inner.end(), msg.begin(), msg.end());
    fix32 ih = SHA256::hash(inner);
    bytes outer = opad;
    outer.insert(outer.end(), ih.begin(), ih.end());
    return SHA256::hash(outer);
}

// ---------------------------------------------------------------
// RFC 6979 — nonce déterministe
// ---------------------------------------------------------------
static u256 bits2int(const fix32& b) {
    u256 v = u256::fromBytes(b);
    const u256& n = N_;
    // si v >= 2^256-? non : on réduit simplement si >= N (bits2octets)
    if (v >= n) v.subMod(n, n); // bits2int strict : shift si bitsenplus; N < 2^256 donc ici on coupe
    return v;
}

static u256 bits2octets(const fix32& b) {
    return u256::mod(u256::fromBytes(b), N_);
}

static fix32 int2octets(const u256& v) { return u256::toBytes(v); }

static void rfc6979Nonce(const bytes& priv, const fix32& h1, u256* outK, size_t count) {
    bytes key = priv;
    if (key.size() < 32) key.insert(key.begin(), 32 - key.size(), 0);
    key.resize(32);
    u256 x; (void)x;
    fix32 z = int2octets(bits2octets(h1));
    bytes V(32, 0x01), K(32, 0x00);
    // K = HMAC(K, V || 0x00 || int2octets(x) || bits2octets(h1))
    bytes step1;
    step1.insert(step1.end(), V.begin(), V.end());
    step1.push_back(0x00);
    step1.insert(step1.end(), key.begin(), key.end());
    step1.insert(step1.end(), z.begin(), z.end());
    fix32 h = hmacSha256(K, step1);
    K.assign(h.begin(), h.end());
    // V = HMAC(K, V)
    h = hmacSha256(K, V);
    V.assign(h.begin(), h.end());
    // K = HMAC(K, V || 0x01 || int2octets(x) || bits2octets(h1))
    bytes step2;
    step2.insert(step2.end(), V.begin(), V.end());
    step2.push_back(0x01);
    step2.insert(step2.end(), key.begin(), key.end());
    step2.insert(step2.end(), z.begin(), z.end());
    h = hmacSha256(K, step2);
    K.assign(h.begin(), h.end());
    // V = HMAC(K, V)
    h = hmacSha256(K, V);
    V.assign(h.begin(), h.end());

    for (size_t c = 0; c < count; ++c) {
        // V = HMAC(K, V); T = V
        do {
            h = hmacSha256(K, V);
            V.assign(h.begin(), h.end());
            u256 cand = u256::fromBytes(V.data(), V.size());
            if (!cand.isZero() && cand < N_) { outK[c] = cand; break; }
            // K = HMAC(K, V || 0x00)
            bytes step3(V.begin(), V.end());
            step3.push_back(0x00);
            h = hmacSha256(K, step3);
            K.assign(h.begin(), h.end());
            h = hmacSha256(K, V);
            V.assign(h.begin(), h.end());
        } while (true);
    }
}

// ---------------------------------------------------------------
// ECDSA
// ---------------------------------------------------------------
Signature ecdsaSign(const bytes& priv, const fix32& msgHash) {
    u256 d = u256::fromBytes(priv.data(), priv.size());
    if (d.isZero() || d >= N_) d = u256::fromDec("1");
    u256 z = bits2int(msgHash);

    Signature sig;
    for (size_t attempt = 0; attempt < 64; ++attempt) {
        u256 k;
        rfc6979Nonce(priv, msgHash, &k, 1);
        // K déjà réduit < N. Ajoute l'index d'attempt pour varier en cas de k inutilisable.
        if (attempt) k.addMod(u256(7 * (attempt + 13)), N_);
        if (k.isZero() || k >= N_) continue;
        PublicKey R = ecMulG(k);
        if (ecIsInfinity(R)) continue;
        u256 r = u256::mod(R.x, N_);
        if (r.isZero()) continue;
        u256 s = u256::invMod(k, N_);
        u256 dr = u256::mulMod(d, r, N_);
        fAdd(dr, z, N_);
        s = u256::mulMod(s, dr, N_);
        if (s.isZero()) continue;
        u256 rec = R.y.bit(0) ? u256(1) : u256(0);
        if (R.x >= N_) rec.addMod(u256(2), u256::max());
        sig.r = u256::toBytes(r);
        sig.s = u256::toBytes(s);
        sig.recid = (uint8_t)rec.l[0];
        return sig;
    }
    memset(sig.r.data(), 0xff, 32);
    memset(sig.s.data(), 0xff, 32);
    sig.recid = 0;
    return sig;
}

bool ecdsaVerify(const fix32& msgHash, const Signature& sig, const PublicKey& pub) {
    const u256& n = N_;
    u256 r = u256::fromBytes(sig.r);
    u256 s = u256::fromBytes(sig.s);
    if (r.isZero() || r >= n || s.isZero() || s >= n) return false;
    u256 z = bits2int(msgHash);
    u256 w = u256::invMod(s, n);
    u256 u1 = u256::mulMod(z, w, n);
    u256 u2 = u256::mulMod(r, w, n);
    PublicKey U1 = ecMulG(u1);
    PublicKey U2 = ecMul(u2, pub);
    PublicKey R = ecAdd(U1, U2);
    if (ecIsInfinity(R)) return false;
    return u256::mod(R.x, n) == r;
}

PublicKey ecdsaRecover(const fix32& msgHash, const Signature& sig) {
    const u256& p = P_;
    const u256& n = N_;
    u256 r = u256::fromBytes(sig.r);
    u256 s = u256::fromBytes(sig.s);
    if (r.isZero() || r >= n || s.isZero() || s >= n) return PublicKey{};
    if (sig.recid > 3) return PublicKey{};

    int head = (sig.recid >> 1) & 1; // x = r + n ?
    int parity = sig.recid & 1;      // parité de y

    PublicKey R;
    bool found = false;
    for (int j = 0; j < 2 && !found; ++j) {
        if (j != head) continue;
        u256 x = r;
        if (j == 1) {
            u256 pmn = p;
            pmn.subMod(n, p); // p - n
            if (r >= pmn) continue; // r + n ≥ p : pas de point valide
            fAdd(x, n, p);
        }
        if (x >= p) continue;
        // y² = x³ + 7
        u256 y2 = u256::mulMod(u256::mulMod(x, x, p), x, p);
        fAdd(y2, u256(7), p);
        u256 e = p; e.addMod(u256(1), p);
        e.shrBits(2);
        u256 y = u256::powMod(y2, e, p);
        if (u256::mulMod(y, y, p) != y2) continue;
        if ((y.bit(0) ? 1 : 0) != parity) {
            if (y.isZero()) continue;
            u256 neg = p;
            neg.subMod(y, p);
            y = neg;
            if ((y.bit(0) ? 1 : 0) != parity) continue;
        }
        R.x = x; R.y = y;
        found = true;
    }
    if (!found) return PublicKey{};

    // Q = r⁻¹ * (s*R - z*G)
    u256 z = bits2int(msgHash);
    u256 rInv = u256::invMod(r, n);
    PublicKey sR = ecMul(s, R);
    PublicKey zG = ecMulG(z);
    PublicKey mzG;
    mzG.x = zG.x;
    mzG.y = p;
    mzG.y.subMod(zG.y, p);
    PublicKey sR_minus = ecAdd(sR, mzG);
    PublicKey Q = ecMul(rInv, sR_minus);
    // contrôle de cohérence : Q doit être sur la courbe
    u256 qy2 = u256::mulMod(u256::mulMod(Q.x, Q.x, p), Q.x, p);
    fAdd(qy2, u256(7), p);
    if (u256::mulMod(Q.y, Q.y, p) != qy2) return PublicKey{};
    return Q;
}

// ---------------------------------------------------------------
// Clés & adresses
// ---------------------------------------------------------------
bytes generatePrivate() {
    const u256& n = N_;
    while (true) {
        bytes r = randomBytes(32);
        if (r.empty()) continue;
        u256 v = u256::fromBytes(r.data(), 32);
        if (!v.isZero() && v < n) return r;
    }
}

PublicKey pubFromPrivate(const bytes& priv) { return ecMulG(u256::fromBytes(priv.data(), priv.size())); }

fix20 addressFromPublicKey(const PublicKey& pub) {
    bytes raw(64);
    fix32 xb = u256::toBytes(pub.x);
    fix32 yb = u256::toBytes(pub.y);
    std::memcpy(raw.data(), xb.data(), 32);
    std::memcpy(raw.data() + 32, yb.data(), 32);
    fix32 h = Keccak256::hash(raw);
    fix20 addr;
    std::memcpy(addr.data(), h.data() + 12, 20);
    return addr;
}

fix20 addressFromPrivate(const bytes& priv) { return addressFromPublicKey(pubFromPrivate(priv)); }

} // namespace nstrike