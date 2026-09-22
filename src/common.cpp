// ------------------------------------------------------------------
// Nstrike — implémentation de l'hex, de u256 (arithmétique 256/512 bits)
// et de l'aléa. Corps de Montgomery utilisé par crypto.cpp.
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#include "common.hpp"

#include <random>

namespace nstrike {

// ---------------------------------------------------------------
// Hex
// ---------------------------------------------------------------
std::string toHex(const void* data, size_t n) {
    static const char* HEX = "0123456789abcdef";
    const uint8_t* p = static_cast<const uint8_t*>(data);
    std::string s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) {
        s.push_back(HEX[p[i] >> 4]);
        s.push_back(HEX[p[i] & 0x0f]);
    }
    return s;
}

std::string toHex(const bytes& b) { return toHex(b.data(), b.size()); }
std::string toHex(const fix32& h) { return toHex(h.data(), h.size()); }
std::string toHex(const fix20& h) { return toHex(h.data(), h.size()); }

static int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

bool fromHex(const std::string& s, bytes& out) {
    std::string t = s;
    if (t.size() >= 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) t = t.substr(2);
    if (t.empty()) { out.clear(); return true; }
    if (t.size() % 2 != 0) t.insert(t.begin(), '0');
    out.resize(t.size() / 2);
    for (size_t i = 0; i < t.size(); i += 2) {
        int hi = hexVal(t[i]), lo = hexVal(t[i + 1]);
        if (hi < 0 || lo < 0) { out.clear(); return false; }
        out[i / 2] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

// ---------------------------------------------------------------
// u256
// ---------------------------------------------------------------
int u256::cmp(const u256& o) const {
    for (int i = 3; i >= 0; --i) {
        if (l[i] < o.l[i]) return -1;
        if (l[i] > o.l[i]) return 1;
    }
    return 0;
}

u256& u256::shlBits(int n) {
    if (n <= 0) return *this;
    if (n >= 256) { for (auto& x : l) x = 0; return *this; }
    int ws = n / 64, bs = n % 64;
    if (bs) {
        uint64_t carry = 0;
        for (int i = 0; i < 4; ++i) {
            uint64_t v = l[i];
            l[i] = (v << bs) | carry;
            carry = (bs == 64) ? 0 : (v >> (64 - bs));
        }
    }
    if (ws) {
        for (int i = 3; i >= ws; --i) l[i] = l[i - ws];
        for (int i = 0; i < ws; ++i) l[i] = 0;
    }
    return *this;
}

u256& u256::shrBits(int n) {
    if (n <= 0) return *this;
    if (n >= 256) { for (auto& x : l) x = 0; return *this; }
    int ws = n / 64, bs = n % 64;
    if (ws) {
        for (int i = 0; i + ws < 4; ++i) l[i] = l[i + ws];
        for (int i = 4 - ws; i < 4; ++i) l[i] = 0;
    }
    if (bs) {
        uint64_t carry = 0;
        for (int i = 3; i >= 0; --i) {
            uint64_t v = l[i];
            l[i] = (v >> bs) | carry;
            carry = (bs == 64) ? 0 : (v << (64 - bs));
        }
    }
    return *this;
}

int u256::bit(int i) const {
    if (i < 0 || i >= 256) return 0;
    return (int)((l[i / 64] >> (i % 64)) & 1);
}

u256& u256::addMod(const u256& b, const u256&) {
    uint64_t carry = 0;
    for (int i = 0; i < 4; ++i) {
        __uint128_t s = (__uint128_t)l[i] + b.l[i] + carry;
        l[i] = (uint64_t)s;
        carry = (uint64_t)(s >> 64);
    }
    return *this;
}

u256& u256::subMod(const u256& b, const u256& mod) {
    if (cmp(b) < 0) addMod(mod, mod);
    uint64_t borrow = 0;
    for (int i = 0; i < 4; ++i) {
        __uint128_t bb = (__uint128_t)b.l[i] + borrow;
        uint64_t d = (uint64_t)((__uint128_t)l[i] - bb);
        borrow = ((__uint128_t)l[i] < bb) ? 1 : 0;
        l[i] = d;
    }
    return *this;
}

void u256::divmod(const u256& a, const u256& b, u256& q, u256& r) {
    u256 aa = a; // copie locale : a et q peuvent aliasser
    q = u256();
    r = u256();
    if (b.isZero()) return; // invalide : diviseur nul
    if (aa.cmp(b) < 0) { r = aa; return; }
    for (int i = 255; i >= 0; --i) {
        r.shlBits(1);
        r.l[0] |= (uint64_t)aa.bit(i);
        if (r >= b) {
            r.subMod(b, b);
            q.l[i / 64] |= (uint64_t(1) << (i % 64));
        }
    }
}

u256 u256::mod(const u256& a, const u256& b) {
    u256 q, r;
    divmod(a, b, q, r);
    return r;
}

static uint64_t lowInverse(uint64_t mod) {
    // calcule -mod^{-1} mod 2^64 (inverse de Newton)
    uint64_t x = 1;
    for (int i = 0; i < 6; ++i) {
        __uint128_t t = (__uint128_t)x * (2 - (__uint128_t)mod * x);
        x = (uint64_t)t;
    }
    return (uint64_t)(0 - x);
}

static u256 pow2mod(int e, const u256& mod) {
    // 2^e mod mod par doublement, en gardant un bit haut explicite :
    // un dépassement 2^256 se perdrait pour mod > 2^255.
    u256 r(1);
    int high = 0; // valeur = r + high*2^256
    for (int i = 0; i < e; ++i) {
        int top = r.bit(255);
        r.shlBits(1);
        high = 2 * high + top;
        while (high > 0 || r >= mod) {
            uint64_t borrow = 0;
            for (int k = 0; k < 4; ++k) {
                __uint128_t bb = (__uint128_t)mod.l[k] + borrow;
                uint64_t d = (uint64_t)((__uint128_t)r.l[k] - bb);
                borrow = ((__uint128_t)r.l[k] < bb) ? 1 : 0;
                r.l[k] = d;
            }
            if (borrow) --high; // emprunt de la partie haute
        }
    }
    return r;
}

u256 u256::mulMod(const u256& a, const u256& b, const u256& mod) {
    return fromMont(montMul(toMont(a, mod), toMont(b, mod), mod), mod);
}

u256 u256::toMont(const u256& a, const u256& mod) {
    u256 r2 = pow2mod(512, mod);
    return montMul(u256::mod(a, mod), r2, mod);
}

u256 u256::fromMont(const u256& a, const u256& mod) { return montMul(a, u256::one(), mod); }

u256 u256::montMul(const u256& a, const u256& b, const u256& mod) {
    // produit t = a*b (512 bits)
    uint64_t t[9] = {0};
    for (int i = 0; i < 4; ++i) {
        if (!a.l[i]) continue;
        uint64_t carry = 0;
        for (int j = 0; j < 4; ++j) {
            __uint128_t cur = (__uint128_t)a.l[i] * b.l[j] + t[i + j] + carry;
            t[i + j] = (uint64_t)cur;
            carry = (uint64_t)(cur >> 64);
        }
        int idx = i + 4;
        while (carry) {
            __uint128_t s = (__uint128_t)t[idx] + carry;
            t[idx] = (uint64_t)s;
            carry = (uint64_t)(s >> 64);
            idx++;
        }
    }
    // réduction de Montgomery
    uint64_t mprime = lowInverse(mod.l[0]);
    for (int i = 0; i < 4; ++i) {
        uint64_t k = t[i] * mprime;
        __uint128_t carry = 0;
        for (int j = 0; j < 4; ++j) {
            __uint128_t cur = (__uint128_t)t[i + j] + (__uint128_t)k * mod.l[j] + carry;
            t[i + j] = (uint64_t)cur;
            carry = cur >> 64;
        }
        int idx = i + 4;
        while (carry) {
            __uint128_t s = (__uint128_t)t[idx] + carry;
            t[idx] = (uint64_t)s;
            carry = s >> 64;
            idx++;
        }
    }
    u256 res;
    res.l = {t[4], t[5], t[6], t[7]};
    // replier le mot haut t[8] : la valeur peut atteindre 2^256+k (mod>2^255),
    // chaque soustraction de mod consomme l'éventuel emprunt de la partie haute.
    uint64_t extra = t[8];
    while (extra > 0 || res >= mod) {
        uint64_t borrow = 0;
        for (int k = 0; k < 4; ++k) {
            __uint128_t bb = (__uint128_t)mod.l[k] + borrow;
            uint64_t d = (uint64_t)((__uint128_t)res.l[k] - bb);
            borrow = ((__uint128_t)res.l[k] < bb) ? 1 : 0;
            res.l[k] = d;
        }
        if (borrow) --extra; // emprunt depuis le mot haut
    }
    return res;
}

u256 u256::powMod(const u256& a, const u256& e, const u256& mod) {
    u256 acc = toMont(u256::one(), mod); // identité (forme R)
    u256 base = toMont(a, mod);          // forme Montgomery requise
    for (int i = 255; i >= 0; --i) {
        acc = montMul(acc, acc, mod);    // mise au carré du résultat
        if (e.bit(i)) acc = montMul(acc, base, mod);
    }
    return fromMont(acc, mod);
}

u256 u256::invMod(const u256& a, const u256& mod) {
    // Fermat : a^(m-2) mod m, valable car p et n sont premiers.
    u256 e = mod;
    e.subMod(u256(2), u256::max()); // mod - 2
    return powMod(a, e, mod);
}

fix32 u256::toBytes(const u256& v) {
    fix32 out{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 8; ++j) {
            out[31 - (i * 8 + j)] = (uint8_t)(v.l[i] >> (8 * j));
        }
    }
    return out;
}

u256 u256::fromBytes(const fix32& b) { return fromBytes(b.data(), 32); }

u256 u256::fromBytes(const uint8_t* p, size_t n) {
    u256 v;
    for (size_t i = 0; i < n; ++i) {
        v.shlBits(8);
        v.l[0] |= p[i];
    }
    return v;
}

u256 u256::fromHex(const std::string& s) {
    bytes b;
    if (!::nstrike::fromHex(s, b)) return u256();
    return fromBytes(b.data(), b.size());
}

u256 u256::fromDec(const std::string& s) {
    u256 v;
    for (char c : s) {
        if (c < '0' || c > '9') continue;
        // v = v*10 + (c-'0')
        uint64_t carry = (uint64_t)(c - '0');
        for (int i = 0; i < 4; ++i) {
            __uint128_t cur = (__uint128_t)v.l[i] * 10 + carry;
            v.l[i] = (uint64_t)cur;
            carry = (uint64_t)(cur >> 64);
        }
    }
    return v;
}

std::string u256::toHexStr() const {
    return "0x" + toHex(toBytes(*this).data(), 32);
}

std::string u256::toDecStr() const {
    if (isZero()) return "0";
    static const u256 ten19(10000000000000000000ULL);
    std::vector<u256> groups;
    u256 q = *this, r;
    while (!q.isZero()) {
        divmod(q, ten19, q, r);
        groups.push_back(r);
    }
    std::string s;
    for (size_t i = groups.size(); i-- > 0;) {
        std::string g = std::to_string(groups[i].l[0]);
        if (i != groups.size() - 1) {
            while (g.size() < 19) g.insert(g.begin(), '0');
        }
        s += g;
    }
    return s;
}

// ---------------------------------------------------------------
// Aléa
// ---------------------------------------------------------------
bytes randomBytes(size_t n) {
    std::random_device rd;
    bytes out(n);
    std::mt19937_64 g(rd());
    for (size_t i = 0; i < n; i += 8) {
        uint64_t v = g();
        for (size_t j = 0; j < 8 && i + j < n; ++j)
            out[i + j] = (uint8_t)(v >> (8 * j));
    }
    return out;
}

} // namespace nstrike