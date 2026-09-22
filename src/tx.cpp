// ------------------------------------------------------------------
// Nstrike — sérialisation canonique d'une transaction (magic + champs big
// endian + données), signature ECDSA et expéditeur par ecrecover.
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#include "tx.hpp"

#include "keccak.hpp"
#include "state.hpp"

namespace nstrike {

namespace {
constexpr unsigned char TX_MAGIC[8] = {'N', 'S', 'T', 'R', 'X', 'T', 'X', 1};

template <typename T>
void put(bytes& out, const T& src, size_t n) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&src);
    out.insert(out.end(), p, p + n);
}
} // namespace

// Sérialisation canonique (hors signature) :
// magic(8) | chainId(4 BE) | nonce(32 BE) | gasPrice(32) | gasLimit(32) |
// create(1) | to(20) | value(32) | dataLen(4 BE) | data
bytes encodeTx(const Transaction& t) {
    bytes out;
    out.insert(out.end(), TX_MAGIC, TX_MAGIC + 8);
    uint32_t cid = t.chainId;
    for (int i = 3; i >= 0; --i) out.push_back(uint8_t(cid >> (8 * i)));
    fix32 nb = u256::toBytes(t.nonce);
    fix32 gp = u256::toBytes(t.gasPrice);
    fix32 gl = u256::toBytes(t.gasLimit);
    fix32 vl = u256::toBytes(t.value);
    out.insert(out.end(), nb.begin(), nb.end());
    out.insert(out.end(), gp.begin(), gp.end());
    out.insert(out.end(), gl.begin(), gl.end());
    out.push_back(t.isCreate ? 1 : 0);
    out.insert(out.end(), t.to.begin(), t.to.end());
    out.insert(out.end(), vl.begin(), vl.end());
    uint32_t dl = static_cast<uint32_t>(t.data.size());
    for (int i = 3; i >= 0; --i) out.push_back(uint8_t(dl >> (8 * i)));
    out.insert(out.end(), t.data.begin(), t.data.end());
    return out;
}

fix32 Transaction::hash() const { return Keccak256::hash(encodeTx(*this)); }

void Transaction::sign(const bytes& priv, const fix32& extraEntropy) {
    fix32 h = hash();
    Signature sig = ecdsaSign(priv, h);
    r = sig.r;
    s = sig.s;
    recid = sig.recid;
    (void)extraEntropy;
}

fix20 Transaction::sender() const {
    Signature sig;
    sig.r = r;
    sig.s = s;
    sig.recid = recid;
    PublicKey pk = ecdsaRecover(hash(), sig);
    return addressFromPublicKey(pk);
}

uint64_t Transaction::upfrontCost() const {
    // gasLimit * gasPrice borné à uint64 (réaliste) ; MVP.
    __uint128_t c = (__uint128_t)gasLimit.l[0] * gasPrice.l[0];
    if (c > ~uint64_t(0)) return ~uint64_t(0);
    return (uint64_t)c;
}

std::string Transaction::toHex() const {
    return "0x" + ::nstrike::toHex(serialize());
}

bytes Transaction::serialize() const {
    bytes all = encodeTx(*this);
    all.insert(all.end(), r.begin(), r.end());
    all.insert(all.end(), s.begin(), s.end());
    all.push_back(recid & 0xFF);
    return all;
}

Transaction Transaction::deserialize(const bytes& raw) {
    return fromHex(::nstrike::toHex(raw));
}

Transaction Transaction::fromHex(const std::string& hex) {
    bytes raw;
    if (!::nstrike::fromHex(hex, raw) || raw.size() < 8 + 4 + 32 + 32 + 32 + 1 + 20 + 32 + 4) {
        throw std::runtime_error("tx: sérialisation invalide");
    }
    Transaction t;
    size_t p = 0;
    for (int i = 0; i < 8; ++i) if (raw[p + i] != TX_MAGIC[i]) throw std::runtime_error("tx: magic invalide");
    p += 8;
    uint32_t cid = 0;
    for (int i = 0; i < 4; ++i) cid = (cid << 8) | raw[p + i];
    p += 4;
    t.chainId = cid;
    t.nonce = u256::fromBytes(raw.data() + p, 32); p += 32;
    t.gasPrice = u256::fromBytes(raw.data() + p, 32); p += 32;
    t.gasLimit = u256::fromBytes(raw.data() + p, 32); p += 32;
    t.isCreate = raw[p] != 0; ++p;
    std::copy(raw.begin() + p, raw.begin() + p + 20, t.to.begin()); p += 20;
    t.value = u256::fromBytes(raw.data() + p, 32); p += 32;
    uint32_t dl = 0;
    for (int i = 0; i < 4; ++i) dl = (dl << 8) | raw[p + i];
    p += 4;
    if (p + dl + 65 > raw.size()) throw std::runtime_error("tx: données tronquées");
    t.data.assign(raw.begin() + p, raw.begin() + p + dl); p += dl;
    if (p + 65 > raw.size()) throw std::runtime_error("tx: signature manquante");
    std::copy(raw.begin() + p, raw.begin() + p + 32, t.r.begin()); p += 32;
    std::copy(raw.begin() + p, raw.begin() + p + 32, t.s.begin()); p += 32;
    t.recid = raw[p];
    return t;
}

} // namespace nstrike