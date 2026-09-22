/**
 * @file common.hpp
 * @brief Types de base, encodage hexadécimal et entier 256 bits (u256)
 * @author Martial Zinsou
 *
 * Ce module définit les fondations de la bibliothèque :
 * - Alias de types (`bytes`, `fix32`, `fix20`)
 * - Constantes zéro (`ADDRESS_ZERO`, `HASH_ZERO`)
 * - Encodage/décodage hexadécimal (RFC 4648, sans 0x obligatoire)
 * - Classe `u256` : entier 256 bits little-endian (4×uint64_t) avec
 *   arithmétique modulaire complète (Montgomery + classique) pour secp256k1.
 *
 * Aucune dépendance externe — tout est implémenté from scratch en C++17.
 */

// ------------------------------------------------------------------
// Nstrike — types de base, encodage hex et entier 256 bits (u256) avec
// arithmétique modulaire de Montgomery pour les corps de courbe.
// Auteur : Martial Zinsou
// Blockchain légère (MVP) écrite from scratch en C++17, sans dépendance.
// ------------------------------------------------------------------
#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "config.hpp"

namespace nstrike {

// ---------------------------------------------------------------
// Alias de types canoniques
// ---------------------------------------------------------------
/** @brief Octets variables (RLP, calldata, bytecode, signatures). */
using bytes = std::vector<uint8_t>;

/** @brief Mot fixe 32 octets (hash, mot u256 big-endian, clé storage). */
using fix32 = std::array<uint8_t, 32>;

/** @brief Adresse fixe 20 octets (160 bits, style Ethereum). */
using fix20 = std::array<uint8_t, 20>;

/** @return Adresse nulle (0x00...00, 20 octets). */
inline const fix20& ADDRESS_ZERO() { static const fix20 z{}; return z; }

/** @return Hash nul (0x00...00, 32 octets). */
inline const fix32& HASH_ZERO() { static const fix32 z{}; return z; }

// ---------------------------------------------------------------
// Encodage hexadécimal (RFC 4648, lowercase, sans 0x obligatoire)
// ---------------------------------------------------------------
/** @brief Encode un blob brut en hexadécimal (minuscules, sans 0x). */
std::string toHex(const void* data, size_t n);

/** @overload */
std::string toHex(const bytes& b);

/** @overload */
std::string toHex(const fix32& h);

/** @overload */
std::string toHex(const fix20& h);

/**
 * @brief Décode une chaîne hexadécimale en octets.
 * @param s Chaîne hex (avec ou sans préfixe "0x"/"0X").
 * @param out Vecteur de sortie (redimensionné).
 * @return true si succès, false si caractère invalide ou longueur impaire.
 *
 * Accepte majuscules/minuscules. Longueur impaire → préfixe '0' implicite.
 */
bool fromHex(const std::string& s, bytes& out);

// ---------------------------------------------------------------
// u256 — Entier 256 bits non signé (little-endian, 4×uint64_t)
// ---------------------------------------------------------------
/**
 * @class u256
 * @brief Entier 256 bits non signé, stockage little-endian (l[0] = mots de poids faible).
 *
 * Représentation : valeur = Σ_{i=0..3} l[i] * 2^{64*i}.
 * Tous les algorithmes sont written from scratch sans bibliothèque big-int.
 *
 * Fonctionnalités :
 * - Comparaisons, décalages, test de bit
 * - Arithmétique modulaire classique (addMod, subMod) — sans retenue >256 bits
 * - Arithmétique de Montgomery (toMont, fromMont, montMul) pour secp256k1
 * - Exponentiation modulaire binaire gauche→droite (powMod)
 * - Inversion modulaire par Fermat (invMod = a^{p-2} mod p)
 * - Division exacte 256 bits (divmod, shift-and-subtract)
 * - Conversion hex/décimal/bytes (big-endian pour I/O)
 * - Génération aléatoire cryptographique (randomBytes)
 *
 * @note Les méthodes `addMod`/`subMod` **ne réduisent pas** modulo 2^256 —
 *       l'appelant doit garantir que le résultat tient dans 256 bits ou
 *       utiliser `mulMod`/`powMod`/`invMod` qui gèrent la réduction.
 */
class u256 {
public:
    /** @brief Mots 64 bits, little-endian : l[0] = bits 0..63, l[3] = bits 192..255. */
    std::array<uint64_t, 4> l{0, 0, 0, 0};

    u256() = default;
    /** @brief Constructeur depuis uint64_t (mots hauts = 0). */
    u256(uint64_t v) { l[0] = v; }

    // --- Parsing & sérialisation ---
    /** @brief Parse hexadécimal (avec ou sans 0x) → u256. */
    static u256 fromHex(const std::string& s);
    /** @brief Parse décimal (string arbitrary length) → u256. */
    static u256 fromDec(const std::string& s);
    /** @brief Sérialise en 32 octets big-endian (pour hashes, adresses, RLP). */
    static fix32 toBytes(const u256& v);
    /** @brief Désérialise depuis 32 octets big-endian. */
    static u256 fromBytes(const fix32& b);
    /** @brief Désérialise depuis n octets big-endian (n ≤ 32, padding gauche zéro). */
    static u256 fromBytes(const uint8_t* p, size_t n);
    /** @return Chaîne hexadécimale avec préfixe "0x" (64 chars + 2). */
    std::string toHexStr() const;
    /** @return Chaîne décimale (arbitrary precision, base 10^19 par groupes). */
    std::string toDecStr() const;

    /** @return true si tous les mots sont nuls. */
    bool isZero() const { return !(l[0] | l[1] | l[2] | l[3]); }

    // --- Comparaisons (ordre non signé) ---
    int cmp(const u256& o) const;        // -1/<, 0/=, 1/>
    bool operator==(const u256& o) const { return cmp(o) == 0; }
    bool operator!=(const u256& o) const { return cmp(o) != 0; }
    bool operator<(const u256& o) const  { return cmp(o) < 0; }
    bool operator>(const u256& o) const  { return cmp(o) > 0; }
    bool operator<=(const u256& o) const { return cmp(o) <= 0; }
    bool operator>=(const u256& o) const { return cmp(o) >= 0; }

    // --- Arithmétique modulaire classique (sans forme Montgomery) ---
    /**
     * @brief Addition modulaire : *this = (*this + b) mod mod.
     * @note Ne propage **pas** la retenue au-delà du bit 255 (comportement EVM).
     *       L'appelant doit s'assurer que *this + b < 2^256 ou accepter le wrap.
     */
    u256& addMod(const u256& b, const u256& mod);

    /**
     * @brief Soustraction modulaire : *this = (*this - b) mod mod.
     * @note Si *this < b, ajoute `mod` avant de soustraire (wrap 256 bits).
     */
    u256& subMod(const u256& b, const u256& mod);

    // --- Décalages & bits ---
    u256& shlBits(int n);   // décalage gauche (bits), n∈[0,255], 0 si n≥256
    u256& shrBits(int n);   // décalage droit (bits), n∈[0,255], 0 si n≥256
    int bit(int i) const;   // bit i (0 = LSB), renvoie 0/1

    // --- Arithmétique de Montgomery (forme R = 2^256) ---
    /**
     * @brief Convertit a (forme normale) → forme Montgomery : a * R mod mod.
     * @param mod Module impair (p ou n de secp256k1).
     */
    static u256 toMont(const u256& a, const u256& mod);
    /**
     * @brief Convertit a (forme Montgomery) → forme normale : a * R^{-1} mod mod.
     */
    static u256 fromMont(const u256& a, const u256& mod);
    /**
     * @brief Multiplication de Montgomery : (a * b * R^{-1}) mod mod.
     * @param a,b Opérandes en forme Montgomery.
     * @return Produit en forme Montgomery.
     * @details Algorithme CIOS (Coarsely Integrated Operand Scanning) :
     *          produit 512 bits → réduction REDC mot par mot avec m' = -mod^{-1} mod 2^64.
     *          Le mot haut t[8] est replié pour les modules > 2^255.
     */
    static u256 montMul(const u256& a, const u256& b, const u256& mod);
    /**
     * @brief Exponentiation modulaire binaire gauche→droite (forme Montgomery).
     * @param a Base (forme normale), e Exposant, mod Module.
     * @return a^e mod mod (forme normale).
     * @details Utilise l'échelle Montgomery : acc = 1_R ; base = a_R ;
     *          pour chaque bit de e (MSB→LSB) : acc = montMul(acc,acc) ; if bit: acc = montMul(acc,base).
     */
    static u256 powMod(const u256& a, const u256& e, const u256& mod);
    /**
     * @brief Inversion modulaire par petit théorème de Fermat : a^{mod-2} mod mod.
     * @pre mod est premier (p ou n de secp256k1).
     */
    static u256 invMod(const u256& a, const u256& mod);
    /** @brief Modulo simple : a mod mod (division exacte, quotient ignoré). */
    static u256 mod(const u256& a, const u256& mod);
    /**
     * @brief Division exacte 256 bits : a = b*q + r, 0 ≤ r < b.
     * @param a Dividende, b Diviseur (≠0), q Quotient (sortie), r Reste (sortie).
     * @details Algorithme shift-and-subtract 256 itérations (MSB→LSB).
     *          Complexité O(256) = constant time pour cryptographie.
     */
    static void divmod(const u256& a, const u256& b, u256& q, u256& r);

    /**
     * @brief Produit modulaire complet : (a * b) mod mod.
     * @details a,b,mod en forme NORMALE. Convertit en Montgomery, multiplie, reconvertit.
     *          Plus lent que montMul direct mais interface plus simple.
     */
    static u256 mulMod(const u256& a, const u256& b, const u256& mod);

    // --- Constantes utiles ---
    static u256 one() { return u256(1); }
    static u256 max() { u256 v; for (auto& x : v.l) x = ~uint64_t(0); return v; }
};

// ---------------------------------------------------------------
// Générateur aléatoire cryptographique
// ---------------------------------------------------------------
/**
 * @brief Remplit `n` octets avec de l'aléa cryptographique (std::random_device + mt19937_64).
 * @note std::random_device est non déterministe sur les OS modernes (getrandom, /dev/urandom).
 *       Pour la production, préférer une source validée (OpenSSL RAND_bytes, etc.).
 */
bytes randomBytes(size_t n);

} // namespace nstrike