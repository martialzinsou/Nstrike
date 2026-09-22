/**
 * @file tx.hpp
 * @brief Transaction Nstrike : sérialisation, signature ECDSA, récupération expéditeur
 * @author Martial Zinsou
 *
 * Format de transaction (inspiré Ethereum, simplifié) :
 * - Champs : nonce, gasPrice, gasLimit, isCreate, to, value, data, chainId
 * - Signature ECDSA secp256k1 (r,s,recid) sur keccak256(sérialisation canonique)
 * - Sérialisation canonique (big-endian, longueur préfixée pour data) :
 *   magic(8) | chainId(4) | nonce(32) | gasPrice(32) | gasLimit(32)
 *   | isCreate(1) | to(20) | value(32) | dataLen(4) | data
 * - Magic : "NSTRXTX1" (8 octets) pour identifier le format
 * - Récupération expéditeur : ecrecover(hash, r, s, recid) → adresse
 * - Upfront cost = gasLimit * gasPrice (vérifié avant exécution)
 */

// ------------------------------------------------------------------
// Nstrike — transaction : champs, sérialisation canonique, signature
// ECDSA et dérivation de l'expéditeur (recover).
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#pragma once

#include "common.hpp"
#include "crypto.hpp"

namespace nstrike {

/**
 * @struct Transaction
 * @brief Transaction Nstrike (transfert ou déploiement de contrat).
 *
 * Champs (ordre de sérialisation = ordre des membres) :
 * - nonce        : compteur du signataire (anti-replay)
 * - gasPrice     : prix du gas en nwei
 * - gasLimit     : gas maximum alloué à cette transaction
 * - isCreate     : true = déploiement (to ignoré, data = bytecode)
 * - to           : destinataire (20 octets, zéro si création)
 * - value        : montant transféré en nwei
 * - data         : payload (appel : selector[4]+args ; création : bytecode brut)
 * - chainId      : identifiant de chaîne (replay protection cross-chain)
 *
 * Signature ECDSA (remplie par sign()) :
 * - r,s,recid    : composantes de la signature sur keccak256(canonique)
 */
struct Transaction {
    u256 nonce;        ///< Nonce du signataire
    u256 gasPrice;     ///< Prix du gas (nwei par unité)
    u256 gasLimit;     ///< Limite de gas
    bool isCreate = false; ///< true = déploiement contrat
    fix20 to{};        ///< Destinataire (0 si création)
    u256 value;        ///< Montant transféré (nwei)
    bytes data;        ///< Données d'appel ou bytecode
    unsigned chainId = Config::CHAIN_ID; ///< Chain ID (EIP-155)

    // Signature ECDSA (remplie par sign())
    fix32 r{};         ///< Composante r (32 octets big-endian)
    fix32 s{};         ///< Composante s (32 octets big-endian)
    uint8_t recid = 0; ///< Recovery ID (0..3)

    // --- Méthodes ---
    /**
     * @brief Hash de la transaction (keccak256 de la sérialisation canonique SANS signature).
     * @return Hash 32 octets (utilisé pour signature et identifiant de tx).
     */
    fix32 hash() const;

    /**
     * @brief Signe la transaction avec une clé privée.
     * @param priv Clé privée 32 octets (big-endian).
     * @param extraEntropy Entropie additionnelle (ignorée, RFC 6979 déterministe).
     * @post Remplit r, s, recid.
     */
    void sign(const bytes& priv, const fix32& extraEntropy = {});

    /**
     * @brief Récupère l'adresse de l'expéditeur depuis la signature.
     * @return Adresse 20 octets (ecrecover).
     */
    fix20 sender() const;

    /** @return true si la transaction porte une signature (r,s non nuls). */
    bool verified() const { return !(r == HASH_ZERO() && s == HASH_ZERO()); }

    /**
     * @brief Coût initial = gasLimit * gasPrice (tronqué à uint64_t).
     * @return Coût en nwei (saturé à UINT64_MAX si overflow).
     */
    uint64_t upfrontCost() const;

    /**
     * @brief Sérialisation canonique AVEC signature (pour stockage bloc).
     * @return Binaire : magic + champs + data + r + s + recid.
     */
    bytes serialize() const;

    /**
     * @brief Désérialisation depuis binaire canonique.
     * @param b Binaire complet (magic + champs + data + sig).
     * @throw std::runtime_error si format invalide.
     */
    static Transaction deserialize(const bytes& b);

    /**
     * @brief Sérialisation hexadécimale (avec préfixe 0x) pour debug/RPC.
     * @return "0x" + hex(serialize()).
     */
    std::string toHex() const;

    /**
     * @brief Parse depuis hexadécimal (avec ou sans 0x).
     * @throw std::runtime_error si invalide.
     */
    static Transaction fromHex(const std::string& hex);
};

} // namespace nstrike