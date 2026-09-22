// ------------------------------------------------------------------
// Nstrike — chaîne de blocs : en-têtes, PoW « léger » (zéros de tête
// Keccak-256), ajustement de difficulté, récompense/halving, mempool et
// application des transactions (virements + appels de contrat via NVM).
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#pragma once

#include <vector>

#include "common.hpp"
#include "state.hpp"
#include "tx.hpp"
#include "vm.hpp"

namespace nstrike {

// ---------------------------------------------------------------
// En-tête de bloc
// ---------------------------------------------------------------
struct BlockHeader {
    uint32_t version = 1;
    uint32_t height = 0;
    uint64_t timestamp = 0;
    u256 difficultyBits = u256(Config::DEFAULT_DIFFICULTY_BITS); // zéros de tête exigés
    fix32 prevHash{};
    fix32 stateRoot{};
    fix32 txRoot{};
    u256 nonce{};   // cherché par le minage
    fix20 miner{};

    fix32 hash() const;             // keccak(sérialisation de l'en-tête)
    bool validPoW() const;          // keccak(hash) a `difficultyBits` zéros de tête
};

// ---------------------------------------------------------------
// Bloc complet
// ---------------------------------------------------------------
struct Block {
    BlockHeader header;
    std::vector<Transaction> txs;
    bytes serialize() const;        // en-tête + transactions (format compact)
    static Block deserialize(const bytes& b);
};

// ---------------------------------------------------------------
// Chaîne
// ---------------------------------------------------------------
class Chain {
public:
    // Registre intégral de la chaîne (MVP, en mémoire).
    std::vector<Block> blocks;
    State state;
    std::vector<Transaction> mempool;

    // --- protocole -------------------------------------------------
    static u256 rewardAt(uint64_t height);                  // avec halving
    static uint32_t nextDifficulty(const std::vector<Block>& blocks, uint64_t now);

    // --- minage & blocs -------------------------------------------
    // Exécute la transaction sur une copie de l'état ; renseigne gasUsed.
    bool runTx(State& st, const Transaction& tx, uint64_t* gasUsed = nullptr) const;
    // Mine `count` blocs pour `miner` (récompense + frais inclus) en tirant
    // le mempool. Retourne le nombre réellement produit.
    size_t mineBlocks(const fix20& miner, size_t count, uint64_t now);
    Block makeBlock(const fix20& miner, uint64_t now, const std::vector<fix32>& txHashes);
    bool validateBlock(const Block& b) const;               // enchaînement + PoW + root

    // --- mempool ---------------------------------------------------
    bool addTx(const Transaction& tx);                      // validation de base
    bool addTxBlocking(const Transaction& tx);              // simulation d'exécution

    // Accesseurs pratiques
    const State& currentState() const { return state; }
    fix32 root() const { return state.stateRoot(); }
};
} // namespace nstrike