// ------------------------------------------------------------------
// Nstrike — secp256k1, ECDSA (RFC 6979) et ecrecover, clés et adresses.
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#pragma once

#include <array>
#include <cstdint>

#include "common.hpp"

namespace nstrike {

// Clé publique secp256k1 (coordonnées affines).
struct PublicKey {
    u256 x, y;
    bool isInfinity() const { return x.isZero() && y.isZero(); }
};

struct Signature {
    fix32 r;
    fix32 s;
    uint8_t recid = 0; // 0..3 (parity + éventuel dépassement de x)
};

// Courbe secp256k1.
const u256& secp256k1P();
const u256& secp256k1N();
const u256& secp256k1Gx();
const u256& secp256k1Gy();

// ----- opérations de courbe (interfaces, arith. interne en Jacobien) -----
PublicKey ecMulG(const u256& k);              // k*G
PublicKey ecMul(const u256& k, const PublicKey& Q);
PublicKey ecAdd(const PublicKey& a, const PublicKey& b);
bool ecIsInfinity(const PublicKey& p);

// ----- ECDSA -----
// Signature déterministe (RFC 6979) de msgHash (32 octets) avec une clé privée
// de 32 octets.
Signature ecdsaSign(const bytes& priv, const fix32& msgHash);
bool ecdsaVerify(const fix32& msgHash, const Signature& sig, const PublicKey& pub);
// Récupère la clé publique depuis (r, s, recid, z) — équivalent de ecrecover.
PublicKey ecdsaRecover(const fix32& msgHash, const Signature& sig);

// ----- clés & adresses -----
bytes generatePrivate();
PublicKey pubFromPrivate(const bytes& priv);
// Adresse Nstrike (20 octets) = keccak256(pubk_64octets)[12..32], style Ethereum.
fix20 addressFromPublicKey(const PublicKey& pub);
fix20 addressFromPrivate(const bytes& priv);

// HMAC-SHA256 (pour RFC 6979).
fix32 hmacSha256(const bytes& key, const bytes& msg);

} // namespace nstrike