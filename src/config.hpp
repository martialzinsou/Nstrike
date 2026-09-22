/**
 * @file config.hpp
 * @brief Constantes globales du protocole Nstrike
 * @author Martial Zinsou
 *
 * Ce fichier centralise toutes les constantes immuables du protocole.
 * Aucune de ces valeurs ne doit être modifiée à l'exécution — elles
 * définissent le consensus. Toute modification nécessiterait un hard fork.
 */

// ------------------------------------------------------------------
// Nstrike — constantes globales du protocole (unité, gas, PoW, récompenses).
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#pragma once

#include <cstdint>

namespace nstrike {

/**
 * @struct Config
 * @brief Ensemble des paramètres de consensus immuables.
 *
 * Tous les membres sont `static constexpr` pour être évalués à la compilation
 * et éviter toute allocation dynamique. Les valeurs suivent le modèle Ethereum
 * avec des ajustements pour un MVP léger.
 */
struct Config {
    /// Nom lisible de la chaîne (utilisé dans les logs, RPC, genesis).
    static constexpr const char* NAME = "Nstrike";

    /// Symbole de la monnaie native (ex: "NST" pour les explorateurs).
    static constexpr const char* SYMBOL = "NST";

    // --------------------------------------------------------------
    // Unité monétaire
    // --------------------------------------------------------------
    /**
     * @brief Exposant de l'unité de base : 1 NST = 10^18 nwei.
     *
     * Identique à Ethereum (1 ETH = 10^18 wei). Toutes les quantités
     * internes (soldes, valeur TX, gasPrice, récompenses) sont exprimées
     * en **nwei** (entiers 256 bits non signés). L'affichage utilisateur
     * divise par 10^18.
     */
    static constexpr int BASE_UNITS_EXPONENT = 18;

    /**
     * @brief Facteur de conversion 10^18 (uint64_t, tient dans 64 bits).
     *
     * Utilisé pour convertir des montants lisibles (double/string) en nwei
     * lors de la construction de transactions via le CLI/RPC.
     */
    static constexpr unsigned long long BASE_UNITS = 1000000000000000000ULL; // 10^18

    // --------------------------------------------------------------
    // Identifiants & tailles fixes
    // --------------------------------------------------------------
    /// Taille d'une adresse (20 octets = 160 bits, style Ethereum).
    static constexpr int ADDRESS_SIZE = 20;

    /// Taille d'un hash (32 octets = 256 bits, sortie SHA-256/Keccak-256).
    static constexpr int HASH_SIZE = 32;

    /**
     * @brief Chain ID (EIP-155 replay protection).
     *
     * Inclus dans la signature des transactions (v = recid + 2*CHAIN_ID + 35).
     * Doit être unique par réseau (mainnet=1, testnet≠1).
     */
    static constexpr unsigned CHAIN_ID = 1;

    // --------------------------------------------------------------
    // Récompense & Halving
    // --------------------------------------------------------------
    /**
     * @brief Récompense de bloc initiale : 50 NST = 5×10¹⁹ nwei.
     *
     * Stockée en **string décimale** car 5×10¹⁹ > 2^64-1 (ne tient pas
     * dans uint64_t). `u256::fromDec()` la parse au démarrage.
     * Halving tous les HALVING_INTERVAL blocs (division exacte par 2).
     */
    static constexpr const char* BASE_REWARD = "50000000000000000000"; // 50 NST en nwei

    /// Intervalle de halving (identique à Bitcoin : 210 000 blocs ≈ 4 ans à 15 s/bloc).
    static constexpr unsigned long long HALVING_INTERVAL = 210000;

    // --------------------------------------------------------------
    // Consensus : temps de bloc & ajustement difficulté
    // --------------------------------------------------------------
    /**
     * @brief Temps cible entre deux blocs (secondes).
     *
     * 15 s = compromis entre finalité rapide et propagation réseau.
     * L'ajustement vise à maintenir cette moyenne sur la fenêtre.
     */
    static constexpr unsigned TARGET_BLOCK_SECONDS = 15;

    /**
     * @brief Fenêtre d'ajustement de difficulté (nombre de blocs).
     *
     * Tous les DIFFICULTY_ADJUST_WINDOW blocs, on compare le temps réel
     * écoulé au temps attendu (WINDOW × TARGET_BLOCK_SECONDS).
     * - Si temps réel > 3× attendu → difficulté –1 bit (plus facile)
     * - Si temps réel < 1/3 attendu → difficulté +1 bit (plus dur)
     * - Sinon inchangé.
     * Bornes : [16, 32] bits.
     */
    static constexpr unsigned DIFFICULTY_ADJUST_WINDOW = 24;

    // --------------------------------------------------------------
    // Proof-of-Work léger
    // --------------------------------------------------------------
    /**
     * @brief Difficulté initiale (bits de zéros de tête requis).
     *
     * Le PoW exige que `Keccak256(Keccak256(header))` ait au moins
     * `DEFAULT_DIFFICULTY_BITS` zéros de poids fort (MSB first).
     * 16 bits par défaut pour un MVP (minage ~secondes sur CPU).
     * Ajusté dynamiquement par la fenêtre ci-dessus.
     */
    static constexpr unsigned DEFAULT_DIFFICULTY_BITS = 16;

    // --------------------------------------------------------------
    // Gas & limites de bloc
    // --------------------------------------------------------------
    /**
     * @brief Gas maximum par bloc (style Ethereum).
     *
     * Limite la quantité de calcul par bloc. 8 000 000 ≈ blocs de ~1-2 Mo
     * avec des transactions simples. MVP : pas de marché de gas (EIP-1559),
     * prix fixe par transaction.
     */
    static constexpr unsigned long long BLOCK_GAS_LIMIT = 8000000ULL;

    /// Nombre max de transactions par bloc (anti-spam, borne mémoire).
    static constexpr unsigned MAX_TX_PER_BLOCK = 256;

    // --------------------------------------------------------------
    // Genesis
    // --------------------------------------------------------------
    /// Timestamp symbolique du bloc 0 (non utilisé pour le consensus).
    static constexpr const char* GENESIS_TIMESTAMP = "Genesis";
};

} // namespace nstrike