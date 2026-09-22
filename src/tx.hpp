// ------------------------------------------------------------------
// Nstrike — transaction : champs, sérialisation canonique, signature
// ECDSA et dérivation de l'expéditeur (recover).
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#pragma once

#include "common.hpp"
#include "crypto.hpp"

namespace nstrike {

// Transaction Nstrike : virement ou création de contrat. Signature type
// Ethereum (ECDSA secp256k1 + recid) sur le hachage Keccak-256 d'une
// sérialisation canonique compacte (variante du RLP, documentée).
struct Transaction {
    u256 nonce;        // compteur du signataire
    u256 gasPrice;     // prix du gas (nwei)
    u256 gasLimit;     // gas alloué
    bool isCreate = false; // true : déploie bytecode (field to ignoré)
    fix20 to{};        // destinataire (ou zero si création)
    u256 value;        // montant transféré (nwei)
    bytes data;        // appel : selector[4] + args ; création : bytecode
    unsigned chainId = Config::CHAIN_ID;

    // signature
    fix32 r{};
    fix32 s{};
    uint8_t recid = 0;

    fix32 hash() const; // keccak de la sérialisation canonique (hors signature)
    void sign(const bytes& priv, const fix32& extraEntropy = {});
    fix20 sender() const; // ecdsaRecover(hash, r, s, recid)
    bool verified() const { return !(r == HASH_ZERO() && s == HASH_ZERO()); }

    uint64_t upfrontCost() const; // gasLimit * gasPrice (en unités, borné 2^256)
    bytes serialize() const;      // canonique + signature (stockage de bloc)
    static Transaction deserialize(const bytes& b);
    std::string toHex() const;    // sérialisation canonique en hex (debug/RPC)
    static Transaction fromHex(const std::string& hex);
};

} // namespace nstrike