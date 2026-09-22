// ------------------------------------------------------------------
// Nstrike — implémentation de l'état (comptes, storage, code) et de ses
// racines. Les racines sont déterministes et indépendantes de l'ordre
// d'insertion : MVP autosuffisant pour la validation des blocs.
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#include "state.hpp"

#include "keccak.hpp"

namespace nstrike {

// ---------------------------------------------------------------
// Comptes
// ---------------------------------------------------------------
Account& State::ensure(const fix20& a) { return accounts_[a]; }

void State::addBalance(const fix20& a, const u256& delta) {
    // addition 256 bits simple (récompense de minage) ; débordement impossible
    // dans la pratique, sinon tryAddBalance est à préférer.
    Account& acc = ensure(a);
    __uint128_t carry = 0;
    for (int i = 0; i < 4; ++i) {
        __uint128_t t = (__uint128_t)acc.balance.l[i] + delta.l[i] + carry;
        acc.balance.l[i] = (uint64_t)t;
        carry = t >> 64;
    }
}

void State::setBalance(const fix20& a, const u256& v) { ensure(a).balance = v; }

bool State::tryAddBalance(const fix20& a, const u256& delta) {
    Account& acc = ensure(a);
    // somme exacte sans débordement 256 bits
    u256 s = delta;
    bool carry = false;
    for (int i = 0; i < 4; ++i) {
        __uint128_t t = (__uint128_t)acc.balance.l[i] + delta.l[i] + (carry ? 1 : 0);
        s.l[i] = (uint64_t)t;
        carry = t >> 64;
    }
    if (carry) return false; // débordement 2^256
    acc.balance = s;
    return true;
}

u256 State::balance(const fix20& a) const {
    auto it = accounts_.find(a);
    return it == accounts_.end() ? u256(0) : it->second.balance;
}

void State::setNonce(const fix20& a, const u256& v) { ensure(a).nonce = v; }
u256 State::nonce(const fix20& a) const {
    auto it = accounts_.find(a);
    return it == accounts_.end() ? u256(0) : it->second.nonce;
}

void State::setCode(const fix20& a, const bytes& code) { ensure(a).code = code; }
const bytes& State::code(const fix20& a) const {
    static const bytes none;
    auto it = accounts_.find(a);
    return it == accounts_.end() ? none : it->second.code;
}

void State::setStorage(const fix20& a, const fix32& key, const u256& value) {
    Account& acc = ensure(a);
    if (value.isZero()) acc.storage.erase(key);
    else acc.storage[key] = value;
}

u256 State::getStorage(const fix20& a, const fix32& key) const {
    auto it = accounts_.find(a);
    if (it == accounts_.end()) return u256(0);
    auto s = it->second.storage.find(key);
    return s == it->second.storage.end() ? u256(0) : s->second;
}

void State::clearStorage(const fix20& a) {
    auto it = accounts_.find(a);
    if (it != accounts_.end()) it->second.storage.clear();
}

bool State::exists(const fix20& a) const { return accounts_.count(a) != 0; }

// ---------------------------------------------------------------
// Raciners
// ---------------------------------------------------------------
fix32 State::codeHash(const fix20& a) const {
    const bytes& c = code(a);
    return c.empty() ? HASH_ZERO() : Keccak256::hash(c);
}

fix32 State::storageRoot(const fix20& a) const {
    auto it = accounts_.find(a);
    if (it == accounts_.end()) return HASH_ZERO();
    fix32 h = HASH_ZERO();
    for (const auto& [k, v] : it->second.storage) {
        bytes leaf;
        leaf.reserve(64);
        leaf.insert(leaf.end(), k.begin(), k.end());
        fix32 vb = u256::toBytes(v);
        leaf.insert(leaf.end(), vb.begin(), vb.end());
        fix32 hk = Keccak256::hash(leaf);
        bytes in;
        in.reserve(64);
        in.insert(in.end(), h.begin(), h.end());
        in.insert(in.end(), hk.begin(), hk.end());
        h = Keccak256::hash(in);
    }
    return h;
}

fix32 State::stateRoot() const {
    fix32 h = HASH_ZERO();
    for (const auto& [addr, acc] : accounts_) {
        if (acc.balance.isZero() && acc.nonce.isZero() && acc.code.empty() && acc.storage.empty())
            continue;
        bytes leaf;
        leaf.reserve(148);
        leaf.insert(leaf.end(), addr.begin(), addr.end());
        fix32 nb = u256::toBytes(acc.nonce);
        fix32 bb = u256::toBytes(acc.balance);
        fix32 ch = codeHash(addr);
        fix32 sr = storageRoot(addr);
        leaf.insert(leaf.end(), nb.begin(), nb.end());
        leaf.insert(leaf.end(), bb.begin(), bb.end());
        leaf.insert(leaf.end(), ch.begin(), ch.end());
        leaf.insert(leaf.end(), sr.begin(), sr.end());
        fix32 lh = Keccak256::hash(leaf);
        bytes in;
        in.reserve(64);
        in.insert(in.end(), h.begin(), h.end());
        in.insert(in.end(), lh.begin(), lh.end());
        h = Keccak256::hash(in);
    }
    return h;
}

fix20 State::deriveContractAddress(const fix20& creator, const u256& nonce) {
    bytes in;
    fix32 nb = u256::toBytes(nonce);
    in.reserve(52);
    in.insert(in.end(), creator.begin(), creator.end());
    in.insert(in.end(), nb.begin(), nb.end());
    fix32 h = Keccak256::hash(in);
    fix20 out;
    std::copy(h.begin() + 12, h.end(), out.begin());
    return out;
}

} // namespace nstrike