/**
 * @file sha256.hpp
 * @brief SHA-256 (FIPS 180-4) implémenté from scratch
 * @author Martial Zinsou
 *
 * Implémentation complète de SHA-256 sans aucune dépendance externe.
 * Conforme à FIPS 180-4 / RFC 6234.
 *
 * Constantes :
 * - 8 mots d'initialisation (racines carrées des 8 premiers nombres premiers)
 * - 64 constantes de tour (racines cubiques des 64 premiers nombres premiers)
 *
 * Padding : 1 bit '1', puis zéros, puis longueur sur 64 bits (big-endian).
 * Taille de bloc : 512 bits (64 octets).
 * Sortie : 256 bits (32 octets, big-endian).
 */

// ------------------------------------------------------------------
// Nstrike — SHA-256 implémenté from scratch (aucune dépendance externe).
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "common.hpp"

namespace nstrike {

/**
 * @class SHA256
 * @brief Hasher SHA-256 incrémental (interface update/digest).
 *
 * Usage typique :
 * @code
 * SHA256 h;
 * h.update(data1);
 * h.update(data2);
 * fix32 digest = h.digest(); // finalise et renvoie le hash
 * @endcode
 *
 * États internes :
 * - h_[8] : registres de hash (32 bits chacun)
 * - total_ : nombre total d'octets traités (pour padding final)
 * - buf_[64] : tampon de bloc partiel
 * - buflen_ : octets valides dans buf_
 */
class SHA256 {
public:
    /** @brief Constructeur : initialise les 8 registres aux constantes FIPS 180-4. */
    SHA256();

    /**
     * @brief Ajoute des données au flux (peut être appelé plusieurs fois).
     * @param data Pointeur vers les données.
     * @param n Nombre d'octets.
     */
    void update(const uint8_t* data, size_t n);

    /** @overload */
    void update(const bytes& b) { update(b.data(), b.size()); }

    /**
     * @brief Finalise le hash (padding + dernier bloc) et renvoie le digest.
     * @return Hash 32 octets (big-endian, MSB first).
     * @note Après digest(), l'objet est réinitialisé pour un nouveau hash.
     */
    fix32 digest();

    /**
     * @brief Hash one-shot (stateless) : hash(data, n).
     * @return Hash 32 octets.
     */
    static fix32 hash(const uint8_t* data, size_t n);

    /** @overload */
    static fix32 hash(const bytes& b) { return hash(b.data(), b.size()); }

private:
    uint32_t h_[8];          ///< Registres de hash (A..H)
    uint64_t total_ = 0;     ///< Octets totaux traités
    uint8_t buf_[64];        ///< Tampon de bloc (512 bits)
    size_t buflen_ = 0;      ///< Octets valides dans buf_

    /**
     * @brief Traite un bloc complet de 64 octets (compression SHA-256).
     * @param p Pointeur vers 64 octets (alignement non requis).
     * @details 64 tours avec constantes K[64] et rotation droite.
     *          Schedule de message : W[t] = σ1(W[t-2]) + W[t-7] + σ0(W[t-15]) + W[t-16].
     */
    void processBlock(const uint8_t* p);
};

/** @brief Raccourci stateless : sha256(bytes) → fix32. */
inline fix32 sha256(const bytes& b) { return SHA256::hash(b); }

} // namespace nstrike