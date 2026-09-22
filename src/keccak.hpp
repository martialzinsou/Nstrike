/**
 * @file keccak.hpp
 * @brief Keccak-256 / SHA3-256 et permutation Keccak-f[1600] (FIPS 202)
 * @author Martial Zinsou
 *
 * Implémentation complète de Keccak-f[1600] (permutation 1600 bits = 25×64 bits)
 * et des fonctions de hachage Keccak-256 (dom=0x01) et SHA3-256 (dom=0x06).
 *
 * Références :
 * - FIPS 202 (SHA-3 Standard)
 * - Keccak Code Package (XKCP) — vecteurs de test officiels
 * - Ethereum Yellow Paper : Keccak-256 pour adresses (dom=0x01, taux 136 octets)
 *
 * Architecture éponge :
 * - État : 1600 bits = 25×64 bits (5×5 lanes)
 * - Taux (rate) : 1088 bits = 136 octets (Keccak-256)
 * - Capacité : 512 bits (sécurité 256 bits)
 * - Padding : multi-rate padding (0x01 || 0x00* || 0x80) pour Keccak,
 *   (0x06 || 0x00* || 0x80) pour SHA3-256.
 *
 * Permutation Keccak-f[1600] (24 tours) :
 *   θ (theta)   : parité des colonnes → XOR lignes
 *   ρ (rho)     : rotation bit à bit par constantes RHO[5][5]
 *   π (pi)      : permutation des lanes (x,y) → (y, 2x+3y)
 *   χ (chi)     : non-linéaire par ligne : a = a ⊕ (¬b ∧ c)
 *   ι (iota)    : XOR constante de tour RC[round] sur lane (0,0)
 */

// ------------------------------------------------------------------
// Nstrike — Keccak-256 (le hachage utilisé par Ethereum pour les adresses)
// et permutation Keccak-f[1600], from scratch.
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <cstdint>

#include "common.hpp"

namespace nstrike {

/**
 * @class Keccak256
 * @brief Hasher Keccak-256 / SHA3-256 incrémental (interface update/digest).
 *
 * @param dom Domaine de padding :
 *        - 0x01 : Keccak-256 (Ethereum, défaut)
 *        - 0x06 : SHA3-256 (NIST FIPS 202)
 *
 * Usage :
 * @code
 * Keccak256 k;           // Keccak-256 (Ethereum)
 * k.update(data);
 * fix32 h = k.digest();
 * @endcode
 *
 * États internes :
 * - st_[25] : état éponge 25×64 bits (little-endian par lane)
 * - idx_ : octets valides dans le tampon de taux (0..135)
 * - dom_ : suffixe de domaine (0x01 ou 0x06)
 */
class Keccak256 {
public:
    /**
     * @brief Constructeur avec domaine de padding.
     * @param dom 0x01 (Keccak-256/Ethereum) ou 0x06 (SHA3-256).
     */
    explicit Keccak256(uint8_t dom = 0x01) : dom_(dom) {}

    /**
     * @brief Absorbe des données dans l'état (peut être appelé plusieurs fois).
     * @param data Pointeur vers les données.
     * @param n Nombre d'octets.
     */
    void update(const uint8_t* data, size_t n);

    /** @overload */
    void update(const bytes& b) { update(b.data(), b.size()); }

    /**
     * @brief Finalise : padding (dom | 0x00* | 0x80), absorption finale,
     *        extraction des 32 premiers octets de l'état (little-endian → big-endian).
     * @return Hash 32 octets (big-endian, MSB first).
     */
    fix32 digest();

    /**
     * @brief Hash one-shot stateless.
     * @param data Données, n Longueur, dom Domaine (0x01/0x06).
     * @return Hash 32 octets big-endian.
     */
    static fix32 hash(const uint8_t* data, size_t n, uint8_t dom = 0x01);

    /** @overload */
    static fix32 hash(const bytes& b, uint8_t dom = 0x01) { return hash(b.data(), b.size(), dom); }

private:
    uint64_t st_[25] = {0};  ///< État éponge 25×64 bits (lane (x,y) = st[x + 5*y])
    size_t idx_ = 0;         ///< Position dans le tampon de taux (0..135)
    uint8_t dom_ = 0x01;     ///< Suffixe de domaine (0x01 Keccak, 0x06 SHA3)

    /**
     * @brief Absorbe un bloc de taux complet (136 octets) et applique Keccak-f[1600].
     * @param p Pointeur vers 136 octets.
     */
    void absorb(const uint8_t* p);
};

/**
 * @brief Permutation Keccak-f[1600] brute (24 tours).
 * @param st État 25×64 bits (modifié in-place).
 * @details Expose le cœur de la permutation pour tests (vecteur XKCP perm(0))
 *          et PoW optionnel sur le noyau Keccak.
 *          Implémentation stricte : θ, ρ, π, χ, ι avec indexation (x,y) = st[x + 5*y].
 */
void keccak_f1600(uint64_t st[25]);

/** @brief Raccourci Keccak-256 (dom=0x01) : keccak256(bytes) → fix32. */
inline fix32 keccak256(const bytes& b) { return Keccak256::hash(b); }

} // namespace nstrike