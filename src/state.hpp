/**
 * @file state.hpp
 * @brief État global (comptes, storage, code, racine d'état canonique)
 * @author Martial Zinsou
 *
 * Modèle de compte style Ethereum :
 * - EOA (Externally Owned Account) : nonce, balance, code vide, storage vide
 * - Contract Account : nonce, balance, bytecode, storage (map clé→valeur)
 *
 * Racine d'état (State Root) :
 * - MVP : racine canonique déterministe sans MPT (Merkle Patricia Trie)
 * - Algorithme : feuilles triées par adresse → concaténation → Keccak cumulatif
 * - Feuille compte = keccak256(addr || nonce(32) || balance(32) || codeHash(32) || storageRoot(32))
 * - storageRoot = keccak cumulatif sur (key(32) || value(32)) triés par clé
 * - codeHash = keccak256(bytecode) ou HASH_ZERO si vide
 * - Avantage : simple, déterministe, indépendant de l'ordre d'insertion
 * - Déviation : pas de preuves d'inclusion compactes (MPT requis pour light clients)
 *
 * Adresse de contrat : keccak256(creator(20) || nonce(32))[12..31]
 * (variante compacte vs RLP Ethereum : rlp([creator, nonce]) → keccak256[12..31])
 */

// ------------------------------------------------------------------
// Nstrike — état global : comptes (nonce, solde), code de contrat et
// storage, avec racine d'état canonique (Keccak cumulatif sur feuilles).
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#pragma once

#include <map>

#include "common.hpp"
#include "crypto.hpp"

namespace nstrike {

// ---------------------------------------------------------------
// Structure de compte
// ---------------------------------------------------------------
/**
 * @struct Account
 * @brief Compte Nstrike (EOA ou contrat).
 * @param nonce   Compteur de transactions (anti-replay, incrémenté par tx).
 * @param balance Solde en nwei (entier 256 bits non signé).
 * @param code    Bytecode du contrat (vide pour EOA).
 * @param storage Map clé→valeur (clé=fix32, valeur=u256), triée par clé.
 */
struct Account {
    u256 nonce;                                      ///< Nonce du compte (incrémenté à chaque tx)
    u256 balance;                                    ///< Solde en nwei
    bytes code;                                      ///< Bytecode déployé (vide pour EOA)
    std::map<fix32, u256, std::less<fix32>> storage; ///< Storage trié par clé (32 octets)
    bool hasCode() const { return !code.empty(); }   ///< true si contrat déployé
};

// ---------------------------------------------------------------
// État global
// ---------------------------------------------------------------
/**
 * @class State
 * @brief État mondial de la chaîne (comptes + racine d'état).
 *
 * L'état est un ensemble ordonné de comptes (map triée par adresse).
 * Toutes les mutations passent par les méthodes publiques qui maintiennent
 * la cohérence. La racine d'état (stateRoot) est calculée à la demande
 * par parcours trié — garantissant l'indépendance de l'ordre d'insertion.
 *
 * Thread-safety : non thread-safe (MVP monothread).
 */
class State {
public:
    // --- Soldes ---
    /**
     * @brief Ajoute `delta` au solde (sans vérification de débordement).
     * @param a Adresse du compte.
     * @param delta Montant à ajouter (nwei).
     * @note Utilisé pour récompenses de minage (overflow impossible en pratique).
     */
    void addBalance(const fix20& a, const u256& delta);

    /**
     * @brief Tente d'ajouter `delta` au solde (avec vérification).
     * @return false si overdraft (balance < delta) ou débordement 2^256.
     */
    bool tryAddBalance(const fix20& a, const u256& delta);

    /** @brief Définit le solde exactement. */
    void setBalance(const fix20& a, const u256& v);

    /** @return Solde actuel (0 si compte inexistant). */
    u256 balance(const fix20& a) const;

    // --- Nonce ---
    void setNonce(const fix20& a, const u256& v);
    u256 nonce(const fix20& a) const;

    // --- Code contrat ---
    void setCode(const fix20& a, const bytes& code);
    const bytes& code(const fix20& a) const;

    // --- Storage (clé 32 octets → valeur u256) ---
    /**
     * @brief Définit une valeur de storage.
     * @param a Adresse du contrat.
     * @param key Clé 32 octets.
     * @param value Valeur (0 = suppression de la clé).
     */
    void setStorage(const fix20& a, const fix32& key, const u256& value);

    /** @return Valeur de storage (0 si absent). */
    u256 getStorage(const fix20& a, const fix32& key) const;

    /** @brief Efface tout le storage d'un contrat. */
    void clearStorage(const fix20& a);

    // --- Introspection ---
    bool exists(const fix20& a) const;
    const std::map<fix20, Account, std::less<fix20>>& accounts() const { return accounts_; }

    // --- Racines d'état (calculées à la demande, déterministes) ---
    /**
     * @brief Racine d'état canonique (Keccak cumulatif sur feuilles triées).
     * @return Hash 32 octets.
     * @details Feuille = keccak(addr || nonce || balance || codeHash || storageRoot).
     *          Parcourt accounts_ trié → concatène feuilles → keccak cumulatif.
     */
    fix32 stateRoot() const;

    /**
     * @brief Racine du storage d'un contrat.
     * @return keccak cumulatif sur (key || value) triés par clé.
     */
    fix32 storageRoot(const fix20& a) const;

    /** @return keccak256(bytecode) ou HASH_ZERO si pas de code. */
    fix32 codeHash(const fix20& a) const;

    // --- Dérivation d'adresse de contrat ---
    /**
     * @brief Adresse de contrat = keccak256(creator || nonce)[12..31].
     * @param creator Adresse du créateur (20 octets).
     * @param nonce Nonce du créateur AVANT incrément (style Ethereum).
     * @return Adresse 20 octets.
     * @note Variante compacte vs RLP Ethereum : rlp([creator, nonce]) → keccak[12..31].
     */
    static fix20 deriveContractAddress(const fix20& creator, const u256& nonce);

private:
    std::map<fix20, Account, std::less<fix20>> accounts_; ///< Comptes triés par adresse
    Account& ensure(const fix20& a);                       ///< Crée si absent, retourne ref
};

} // namespace nstrike