/**
 * @file crypto.hpp
 * @brief Cryptographie secp256k1 : ECDSA (RFC 6979), ecrecover, clés, adresses
 * @author Martial Zinsou
 *
 * Implémentation complète de secp256k1 from scratch :
 * - Corps premier p = 2^256 - 2^32 - 977
 * - Ordre du sous-groupe n = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBD25E8CD0364141
 * - Générateur G = (Gx, Gy) standard
 * - Coordonnées affines (x,y) pour l'API publique
 * - Arithmétique interne en coordonnées de Jacobien (X,Y,Z) pour éviter les inversions
 *   (conversion affine↔Jacobien uniquement aux frontières)
 * - Formules d'addition/doublage en Jacobien (formules "add-2007-bl" et "dbl-2007-bl")
 * - ECDSA déterministe RFC 6979 (HMAC-SHA256 pour génération k)
 * - Récupération de clé publique (ecrecover) via recid (0..3)
 * - Adresses : keccak256(0x04 || x || y)[12..31] (non-compressed, style Ethereum)
 *
 * Aucune dépendance externe — tout arithmétique via u256 (Montgomery + classique).
 */

// ------------------------------------------------------------------
// Nstrike — secp256k1, ECDSA (RFC 6979) et ecrecover, clés et adresses.
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#pragma once

#include <array>
#include <cstdint>

#include "common.hpp"

namespace nstrike {

// ---------------------------------------------------------------
// Structures de données publiques
// ---------------------------------------------------------------
/**
 * @struct PublicKey
 * @brief Clé publique secp256k1 en coordonnées affines (x,y).
 * @note Point à l'infini = (0,0) — détecté par isInfinity().
 */
struct PublicKey {
    u256 x, y;  ///< Coordonnées affines (big-endian logical, u256 little-endian storage)
    bool isInfinity() const { return x.isZero() && y.isZero(); }
};

/**
 * @struct Signature
 * @brief Signature ECDSA (r, s, recid).
 * @param r  Composante r (32 octets big-endian).
 * @param s  Composante s (32 octets big-endian, s ≤ n/2 pour low-s).
 * @param recid  Recovery ID (0..3) : parity de y + bit de dépassement de x.
 */
struct Signature {
    fix32 r;          ///< Composante r (32 octets)
    fix32 s;          ///< Composante s (32 octets)
    uint8_t recid = 0; ///< 0..3 : (y_parity) | (x_overflow<<1)
};

// ---------------------------------------------------------------
// Paramètres de courbe secp256k1 (constantes globales)
// ---------------------------------------------------------------
/** @brief Premier du corps de base : p = 2^256 - 2^32 - 977. */
const u256& secp256k1P();

/** @brief Ordre du sous-groupe généré par G : n (premier). */
const u256& secp256k1N();

/** @brief Coordonnée x du générateur G (standard SEC 2). */
const u256& secp256k1Gx();

/** @brief Coordonnée y du générateur G (standard SEC 2). */
const u256& secp256k1Gy();

// ---------------------------------------------------------------
// Opérations de courbe (interne : Jacobien ; API : affine)
// ---------------------------------------------------------------
/**
 * @brief Multiplication scalaire du générateur : k * G.
 * @param k Scalaire (0 < k < n).
 * @return Point affine k*G.
 * @details Utilise échelle binaire gauche→droite en coordonnées de Jacobien.
 */
PublicKey ecMulG(const u256& k);

/**
 * @brief Multiplication scalaire d'un point arbitraire : k * Q.
 * @param k Scalaire, Q Point affine.
 * @return k*Q en affine.
 */
PublicKey ecMul(const u256& k, const PublicKey& Q);

/**
 * @brief Addition de deux points affines : a + b.
 * @return a + b en affine (gère point à l'infini, doublage si a==b).
 */
PublicKey ecAdd(const PublicKey& a, const PublicKey& b);

/** @return true si point à l'infini (x=0,y=0). */
bool ecIsInfinity(const PublicKey& p);

// ---------------------------------------------------------------
// ECDSA (RFC 6979 déterministe)
// ---------------------------------------------------------------
/**
 * @brief Signe un hash de message (32 octets) avec une clé privée (32 octets).
 * @param priv Clé privée 32 octets (big-endian, 0 < priv < n).
 * @param msgHash Hash du message (32 octets, ex: keccak256(tx)).
 * @return Signature (r,s,recid) avec s ≤ n/2 (low-s, BIP-62).
 * @details Génération k déterministe RFC 6979 :
 *   - HMAC-SHA256 avec clé = priv, message = msgHash
 *   - Boucle HMAC-DRBG jusqu'à 0 < k < n
 *   - Signature : r = (k*G).x mod n, s = k^{-1}(z + r*d) mod n
 *   - recid = (y_parity) | (x_overflow<<1) pour ecrecover
 */
Signature ecdsaSign(const bytes& priv, const fix32& msgHash);

/**
 * @brief Vérifie une signature ECDSA.
 * @param msgHash Hash du message signé (32 octets).
 * @param sig Signature (r,s,recid).
 * @param pub Clé publique du signataire (affine).
 * @return true si signature valide.
 * @details Vérification standard :
 *   w = s^{-1} mod n, u1 = z*w mod n, u2 = r*w mod n
 *   R = u1*G + u2*pub ; valide si R.x mod n == r
 */
bool ecdsaVerify(const fix32& msgHash, const Signature& sig, const PublicKey& pub);

/**
 * @brief Récupère la clé publique depuis (msgHash, r, s, recid) — ecrecover.
 * @param msgHash Hash du message (32 octets).
 * @param sig Signature (r,s,recid).
 * @return Clé publique affine (ou point à l'infini si échec).
 * @details Algorithme de récupération (secp256k1) :
 *   - x = r + (recid & 2 ? n : 0)  (gérer dépassement x ≥ n)
 *   - y = sqrt(x^3 + 7) mod p avec parité = recid & 1
 *   - R = (x,y) ; e = msgHash (comme entier)
 *   - Q = r^{-1} (s*R - e*G)
 */
PublicKey ecdsaRecover(const fix32& msgHash, const Signature& sig);

// ---------------------------------------------------------------
// Génération de clés & adresses
// ---------------------------------------------------------------
/**
 * @brief Génère une clé privée aléatoire (32 octets, 0 < priv < n).
 * @return Clé privée 32 octets (big-endian).
 */
bytes generatePrivate();

/**
 * @brief Dérive la clé publique depuis une clé privée.
 * @param priv Clé privée 32 octets.
 * @return Clé publique affine = priv * G.
 */
PublicKey pubFromPrivate(const bytes& priv);

/**
 * @brief Calcule l'adresse Nstrike (20 octets) depuis une clé publique.
 * @param pub Clé publique affine.
 * @return Adresse = keccak256(0x04 || x || y)[12..31] (non-compressed).
 */
fix20 addressFromPublicKey(const PublicKey& pub);

/**
 * @brief Adresse depuis clé privée (raccourci : pubFromPrivate + addressFromPublicKey).
 */
fix20 addressFromPrivate(const bytes& priv);

// ---------------------------------------------------------------
// HMAC-SHA256 (RFC 6979 / RFC 4868)
// ---------------------------------------------------------------
/**
 * @brief HMAC-SHA256 (clé arbitraire, message arbitraire).
 * @param key Clé (arbitrary length).
 * @param msg Message (arbitrary length).
 * @return HMAC 32 octets.
 * @details HMAC(K,m) = SHA256((K⊕opad) || SHA256((K⊕ipad) || m))
 *          ipad = 0x36..., opad = 0x5c... (bloc 64 octets).
 */
fix32 hmacSha256(const bytes& key, const bytes& msg);

} // namespace nstrike