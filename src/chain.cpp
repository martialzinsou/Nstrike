// ------------------------------------------------------------------
// Nstrike — implémentation de la chaîne : fonction de hachage d'en-tête,
// PoW par zéros de tête, ajustement de difficulté, récompense avec
// halving, mempool et exécution des transactions (gas + NVM).
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#include "chain.hpp"

#include <algorithm>

#include "keccak.hpp"

namespace nstrike {

// ---------------------------------------------------------------
// En-tête : sérialisation et PoW
// ---------------------------------------------------------------
namespace {
void wr(bytes& o, uint32_t v) { for (int i = 3; i >= 0; --i) o.push_back(uint8_t(v >> (8 * i))); }
void wr(bytes& o, uint64_t v) { for (int i = 7; i >= 0; --i) o.push_back(uint8_t(v >> (8 * i))); }
uint32_t rd32(const bytes& b, size_t& p) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v = (v << 8) | b[p + i];
    p += 4;
    return v;
}
uint64_t rd64(const bytes& b, size_t& p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | b[p + i];
    p += 8;
    return v;
}

fix32 cumulativeRoot(const std::vector<fix32>& leaves) {
    fix32 h = HASH_ZERO();
    for (const fix32& l : leaves) {
        bytes in;
        in.reserve(64);
        in.insert(in.end(), h.begin(), h.end());
        in.insert(in.end(), l.begin(), l.end());
        h = Keccak256::hash(in);
    }
    return h;
}

// k bits de poids forts == 0 ?
bool leadingZeroBits(const fix32& h, uint64_t bits) {
    for (uint64_t i = 0; i < bits; ++i) {
        uint8_t byte = h[i / 8];
        uint8_t mask = uint8_t(1u << (7 - (i % 8)));
        if (byte & mask) return false;
    }
    return true;
}
} // namespace

fix32 BlockHeader::hash() const {
    bytes o;
    wr(o, version);
    wr(o, height);
    wr(o, timestamp);
    fix32 db = u256::toBytes(difficultyBits);
    o.insert(o.end(), db.begin(), db.end());
    o.insert(o.end(), prevHash.begin(), prevHash.end());
    o.insert(o.end(), stateRoot.begin(), stateRoot.end());
    o.insert(o.end(), txRoot.begin(), txRoot.end());
    o.insert(o.end(), miner.begin(), miner.end());
    fix32 nb = u256::toBytes(nonce);
    o.insert(o.end(), nb.begin(), nb.end());
    return Keccak256::hash(o);
}

bool BlockHeader::validPoW() const {
    fix32 h = Keccak256::hash(bytes(hash().begin(), hash().end()));
    return leadingZeroBits(h, difficultyBits.l[0]);
}

// ---------------------------------------------------------------
// Sérialisation de bloc
// ---------------------------------------------------------------
bytes Block::serialize() const {
    bytes o;
    wr(o, header.version);
    wr(o, header.height);
    wr(o, header.timestamp);
    fix32 db = u256::toBytes(header.difficultyBits);
    fix32 nb = u256::toBytes(header.nonce);
    o.insert(o.end(), db.begin(), db.end());
    o.insert(o.end(), header.prevHash.begin(), header.prevHash.end());
    o.insert(o.end(), header.stateRoot.begin(), header.stateRoot.end());
    o.insert(o.end(), header.txRoot.begin(), header.txRoot.end());
    o.insert(o.end(), header.miner.begin(), header.miner.end());
    o.insert(o.end(), nb.begin(), nb.end());
    wr(o, (uint32_t)txs.size());
    for (const Transaction& t : txs) {
        bytes ts = t.serialize();
        wr(o, (uint32_t)ts.size());
        o.insert(o.end(), ts.begin(), ts.end());
    }
    return o;
}

Block Block::deserialize(const bytes& b) {
    Block blk;
    size_t p = 0;
    if (b.size() < 4 + 4 + 8 + 32 + 32 + 32 + 32 + 20 + 32 + 4) throw std::runtime_error("bloc tronqué");
    blk.header.version = rd32(b, p);
    blk.header.height = rd32(b, p);
    blk.header.timestamp = rd64(b, p);
    blk.header.difficultyBits = u256::fromBytes(b.data() + p, 32); p += 32;
    std::copy(b.begin() + p, b.begin() + p + 32, blk.header.prevHash.begin()); p += 32;
    std::copy(b.begin() + p, b.begin() + p + 32, blk.header.stateRoot.begin()); p += 32;
    std::copy(b.begin() + p, b.begin() + p + 32, blk.header.txRoot.begin()); p += 32;
    std::copy(b.begin() + p, b.begin() + p + 20, blk.header.miner.begin()); p += 20;
    blk.header.nonce = u256::fromBytes(b.data() + p, 32); p += 32;
    uint32_t ntx = rd32(b, p);
    blk.txs.reserve(ntx);
    for (uint32_t i = 0; i < ntx && p + 4 <= b.size(); ++i) {
        uint32_t len = rd32(b, p);
        if (p + len > b.size()) throw std::runtime_error("bloc: transaction tronquée");
        bytes body(b.begin() + p, b.begin() + p + len);
        p += len;
        blk.txs.push_back(Transaction::deserialize(body));
    }
    return blk;
}

// ---------------------------------------------------------------
// Protocole : récompense & difficulté
// ---------------------------------------------------------------
u256 Chain::rewardAt(uint64_t height) {
    u256 r = u256::fromDec(Config::BASE_REWARD); // 50 NST en nwei (string 256 bits)
    uint64_t halvings = height / Config::HALVING_INTERVAL;
    for (uint64_t i = 0; i < halvings; ++i) r.shrBits(1);
    return r;
}

uint32_t Chain::nextDifficulty(const std::vector<Block>& blocks, uint64_t now) {
    const uint64_t W = Config::DIFFICULTY_ADJUST_WINDOW;
    if (blocks.empty()) return Config::DEFAULT_DIFFICULTY_BITS;
    uint32_t cur = (uint32_t)blocks.back().header.difficultyBits.l[0];
    if (blocks.size() <= W) return cur;
    uint64_t firstTs = blocks[blocks.size() - W - 1].header.timestamp;
    uint64_t lastTs = blocks.back().header.timestamp;
    uint64_t actual = lastTs > firstTs ? lastTs - firstTs : 0;
    uint64_t expected = W * Config::TARGET_BLOCK_SECONDS;
    if (actual > expected * 3) return cur > 16 ? cur - 1 : cur;
    if (actual < expected / 3) return cur < 32 ? cur + 1 : cur;
    return cur;
}

// ---------------------------------------------------------------
// Exécution d'une transaction (sur une copie d'état fournie)
// ---------------------------------------------------------------
namespace {
// produit 512 bits : faux si gasLimit*gasPrice dépasse 2^256
bool mul128(u256 a, u256 b, uint64_t* out) {
    __uint128_t t = (__uint128_t)a.l[0] * b.l[0];
    if (!(t >> 64) && a.l[1] == 0 && b.l[1] == 0) { *out = (uint64_t)t; return true; }
    return false;
}
constexpr uint64_t BASE_TX_GAS = 21000;   // virement simple
constexpr uint64_t CREATE_GAS = 32000;    // création de contrat
constexpr uint64_t DATA_GAS = 100;        // par octet de data
} // namespace

bool Chain::runTx(State& st, const Transaction& tx, uint64_t* gasUsedOut) const {
    if (tx.chainId != Config::CHAIN_ID) return false;
    fix20 from = tx.sender();
    u256 nonce = st.nonce(from);

    // nonce et solde de départ
    if (tx.nonce != nonce) return false;
    uint64_t upfront = 0;
    if (!mul128(tx.gasPrice, tx.gasLimit, &upfront)) return false; // prix irréaliste
    u256 need = tx.value;
    {
        __uint128_t t = (__uint128_t)need.l[0] + upfront;  // upfront < 2^64
        need.l[0] = (uint64_t)t;
        bool c2 = t >> 64;
        for (int i = 1; i < 4 && c2; ++i) {
            __uint128_t s = (__uint128_t)need.l[i] + 1;
            need.l[i] = (uint64_t)s;
            c2 = s >> 64;
        }
        if (c2) return false; // débordement 2^256 impossible à couvrir
    }
    if (st.balance(from) < need) return false;

    // prélever le coût initial et avancer le nonce
    u256 bal = st.balance(from);
    for (int i = 0; i < 4; ++i) {
        __uint128_t t = (__uint128_t)bal.l[i] - need.l[i];
        bal.l[i] = (uint64_t)t;
    }
    st.setBalance(from, bal);
    st.setNonce(from, u256(nonce.l[0] + 1));

    uint64_t consumed = BASE_TX_GAS + DATA_GAS * (uint64_t)tx.data.size();
    if (tx.isCreate) consumed += CREATE_GAS + 200 * (uint64_t)tx.data.size();
    uint64_t startGas = tx.gasLimit.l[0] > consumed ? tx.gasLimit.l[0] - consumed : 0;

    if (tx.isCreate) {
        // déploiement : adresse dérivée du créateur + nonce (pré-incrément)
        fix20 addr = State::deriveContractAddress(from, nonce);
        st.setCode(addr, tx.data);
        if (!tx.value.isZero()) st.tryAddBalance(addr, tx.value);
    } else if (tx.to != ADDRESS_ZERO() && !st.code(tx.to).empty() && !tx.data.empty()) {
        // appel de contrat : le gas restant alimente la VM
        CallContext ctx;
        ctx.self = tx.to;
        ctx.origin = from;
        ctx.caller = from;
        ctx.callvalue = tx.value;
        ctx.calldata = tx.data;
        VMResult r = vmExecute(st, ctx, startGas);
        if (!r.ok) return false;                      // révert/out-of-gas -> tx rejetée
        consumed += (startGas - r.gasUsed);
    } else if (tx.to != ADDRESS_ZERO()) {
        if (!tx.value.isZero() && !st.tryAddBalance(tx.to, tx.value)) return false;
    }
    // rembourser le gas non consommé
    uint64_t unused = tx.gasLimit.l[0] > consumed ? tx.gasLimit.l[0] - consumed : 0;
    __uint128_t gross = (__uint128_t)unused * tx.gasPrice.l[0];
    if (gross) {
        u256 b = st.balance(from);
        uint64_t add0 = (uint64_t)(gross & ~uint64_t(0));
        uint64_t add1 = (uint64_t)(gross >> 64);
        __uint128_t t = (__uint128_t)b.l[0] + add0;
        b.l[0] = (uint64_t)t;
        uint64_t c2 = (uint64_t)(t >> 64);
        t = (__uint128_t)b.l[1] + add1 + c2;
        b.l[1] = (uint64_t)t;
        for (int i = 2; i < 4; ++i) {
            if (!(t >> 64)) break;
            t = (__uint128_t)b.l[i] + 1;
            b.l[i] = (uint64_t)t;
        }
        st.setBalance(from, b);
    }
    if (gasUsedOut) *gasUsedOut = consumed;
    return true;
}

// ---------------------------------------------------------------
// Mempool
// ---------------------------------------------------------------
bool Chain::addTx(const Transaction& tx) {
    if (tx.chainId != Config::CHAIN_ID) return false;
    if (!tx.verified()) return false;
    State probe = state;
    uint64_t used = 0;
    if (!runTx(probe, tx, &used)) return false;
    mempool.push_back(tx);
    return true;
}

bool Chain::addTxBlocking(const Transaction& tx) { return addTx(tx); }

// ---------------------------------------------------------------
// Minage
// ---------------------------------------------------------------
size_t Chain::mineBlocks(const fix20& miner, size_t count, uint64_t now) {
    size_t mined = 0;
    while (mined < count) {
        uint32_t height = (uint32_t)blocks.size();
        fix32 prev = blocks.empty() ? HASH_ZERO() : blocks.back().header.hash();

        // choisir les transactions du mempool (validées une à une)
        State target = state;
        std::vector<Transaction> chosen;
        std::vector<fix32> txHashes;
        u256 fees;
        uint64_t gasAvail = Config::BLOCK_GAS_LIMIT;
        for (const Transaction& tx : mempool) {
            if (chosen.size() >= Config::MAX_TX_PER_BLOCK) break;
            uint64_t used = 0;
            State probe = target;
            if (!runTx(probe, tx, &used)) continue;
            if (used > gasAvail) continue;
            gasAvail -= used;
            target = probe;
            chosen.push_back(tx);
            txHashes.push_back(tx.hash());
            // frais de gas = used * gasPrice, cumulés (frais du mineur)
            __uint128_t f = (__uint128_t)used * tx.gasPrice.l[0];
            u256 fw;
            fw.l[0] = (uint64_t)(f & ~uint64_t(0));
            fw.l[1] = (uint64_t)(f >> 64);
            bool c2 = false;
            for (int i = 0; i < 4; ++i) {
                __uint128_t s = (__uint128_t)fees.l[i] + fw.l[i] + (c2 ? 1 : 0);
                fees.l[i] = (uint64_t)s;
                c2 = s >> 64;
            }
        }

        // récompense au mineur + frais
        u256 reward = rewardAt(height);
        target.addBalance(miner, reward);
        if (!fees.isZero()) target.addBalance(miner, fees);

        Block b;
        b.header.version = 1;
        b.header.height = height;
        b.header.timestamp = now;
        b.header.difficultyBits = u256(Chain::nextDifficulty(blocks, now));
        b.header.prevHash = prev;
        b.header.stateRoot = target.stateRoot();
        b.header.txRoot = cumulativeRoot(txHashes);
        b.header.miner = miner;
        b.txs = chosen;
        // PoW : pré-calculer la partie fixe de l'en-tête, ne mettre à jour que le nonce
        bytes headerFixed;
        {
            bytes o;
            wr(o, b.header.version);
            wr(o, b.header.height);
            wr(o, b.header.timestamp);
            fix32 db = u256::toBytes(b.header.difficultyBits);
            o.insert(o.end(), db.begin(), db.end());
            o.insert(o.end(), b.header.prevHash.begin(), b.header.prevHash.end());
            o.insert(o.end(), b.header.stateRoot.begin(), b.header.stateRoot.end());
            o.insert(o.end(), b.header.txRoot.begin(), b.header.txRoot.end());
            o.insert(o.end(), b.header.miner.begin(), b.header.miner.end());
            // espace pour le nonce (32 octets)
            o.insert(o.end(), 32, 0);
            headerFixed = std::move(o);
        }
        for (uint64_t n = 0;; ++n) {
            fix32 nb = u256::toBytes(u256(n));
            std::copy(nb.begin(), nb.end(), headerFixed.end() - 32);
            fix32 h = Keccak256::hash(headerFixed);
            if (leadingZeroBits(Keccak256::hash(bytes(h.begin(), h.end())), b.header.difficultyBits.l[0])) {
                b.header.nonce = u256(n);
                break;
            }
        }
        blocks.push_back(b);
        state = target;
        // retirer les txs minées du mempool
        std::vector<Transaction> rest;
        for (const Transaction& tx : mempool) {
            if (std::find(txHashes.begin(), txHashes.end(), tx.hash()) == txHashes.end())
                rest.push_back(tx);
        }
        mempool = std::move(rest);
        ++mined;
    }
    return mined;
}

// ---------------------------------------------------------------
// Validation d'un bloc (enchaînement + PoW)
// ---------------------------------------------------------------
bool Chain::validateBlock(const Block& b) const {
    if (b.header.height != blocks.size()) return false;
    fix32 prev = blocks.empty() ? HASH_ZERO() : blocks.back().header.hash();
    if (b.header.prevHash != prev) return false;
    if (!b.header.validPoW()) return false;
    return true;
}

} // namespace nstrike