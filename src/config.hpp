// ------------------------------------------------------------------
// Nstrike — constantes globales du protocole (unité, gas, PoW, récompenses).
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#pragma once

namespace nstrike {

// Protocole Nstrike — constantes globales.
struct Config {
    static constexpr const char* NAME = "Nstrike";
    static constexpr const char* SYMBOL = "NST";
    // Unité de base : 1 NST = 1e18 unités (nwei), comme le wei d'Ethereum.
    static constexpr int BASE_UNITS_EXPONENT = 18;
    static constexpr unsigned long long BASE_UNITS = 1000000000000000000ULL;

    // Adresses : 20 octets (style Ethereum).
    static constexpr int ADDRESS_SIZE = 20;
    // Hash : 32 octets.
    static constexpr int HASH_SIZE = 32;

    // Identifiant de chaîne (rejoue anti-relecture de transactions).
    static constexpr unsigned CHAIN_ID = 1;

    // Récompense de bloc : 50 NST (repréé sur 256 bits), halving tous les 210 000 blocs.
    static constexpr const char* BASE_REWARD = "50000000000000000000"; // 50 NST
    static constexpr unsigned long long HALVING_INTERVAL = 210000;

    // Temps de bloc cible (secondes) pour l'ajustement de difficulté.
    static constexpr unsigned TARGET_BLOCK_SECONDS = 15;
    static constexpr unsigned DIFFICULTY_ADJUST_WINDOW = 24;

    // PoW « léger » : bits de zéros de tête exigés par défaut. Réglable / bloc.
    static constexpr unsigned DEFAULT_DIFFICULTY_BITS = 16;

    // Gas : plafonds d'un bloc.
    static constexpr unsigned long long BLOCK_GAS_LIMIT = 8000000ULL;
    static constexpr unsigned MAX_TX_PER_BLOCK = 256;

    static constexpr const char* GENESIS_TIMESTAMP = "Genesis";
};

} // namespace nstrike