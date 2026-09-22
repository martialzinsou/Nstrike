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

// Compte Nstrike (style Ethereum) : nonce, solde, code (contrat), storage.
struct Account {
    u256 nonce;
    u256 balance;
    bytes code;                                  // vide pour un compte EOA
    std::map<fix32, u256, std::less<fix32>> storage;
    bool hasCode() const { return !code.empty(); }
};

// État global. MVP : racine canonique déterministe (liste triée de feuilles
// concaténées puis hachées par Keccak-256) — équivalent trivé d'un MPT,
// self-consistente pour la validation de blocs.
class State {
public:
    void addBalance(const fix20& a, const u256& delta);
    bool tryAddBalance(const fix20& a, const u256& delta); // false si overdraft ou débordement
    void setBalance(const fix20& a, const u256& v);
    u256 balance(const fix20& a) const;

    void setNonce(const fix20& a, const u256& v);
    u256 nonce(const fix20& a) const;

    void setCode(const fix20& a, const bytes& code);
    const bytes& code(const fix20& a) const;

    void setStorage(const fix20& a, const fix32& key, const u256& value);
    u256 getStorage(const fix20& a, const fix32& key) const;
    void clearStorage(const fix20& a);

    bool exists(const fix20& a) const;
    const std::map<fix20, Account, std::less<fix20>>& accounts() const { return accounts_; }

    // Raciners d'état (indépendantes de l'ordre d'insertion).
    fix32 stateRoot() const;
    fix32 storageRoot(const fix20& a) const;
    fix32 codeHash(const fix20& a) const;

    // Adresse de contrat = keccak256(createur || nonce)[12..32] (style Ethereum,
    // variante compacte au lieu de RLP).
    static fix20 deriveContractAddress(const fix20& creator, const u256& nonce);

private:
    std::map<fix20, Account, std::less<fix20>> accounts_;
    Account& ensure(const fix20& a);
};

} // namespace nstrike