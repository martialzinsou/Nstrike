// ------------------------------------------------------------------
// Nstrike — types de base, encodage hex et entier 256 bits (u256) avec
// arithmétique modulaire de Montgomery pour les corps de courbe.
// Auteur : Martial Zinsou
// Chaîne de blocs légère (MVP) écrite from scratch en C++17, sans dépendance.
// ------------------------------------------------------------------
#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "config.hpp"

namespace nstrike {

using bytes = std::vector<uint8_t>;
using fix32 = std::array<uint8_t, 32>;
using fix20 = std::array<uint8_t, 20>;

inline const fix20& ADDRESS_ZERO() { static const fix20 z{}; return z; }
inline const fix32& HASH_ZERO() { static const fix32 z{}; return z; }

// ---------------------------------------------------------------
// Hex
// ---------------------------------------------------------------

// ascii héxadécimal d'un blob.
std::string toHex(const void* data, size_t n);
std::string toHex(const bytes& b);
std::string toHex(const fix32& h);
std::string toHex(const fix20& h);

// hex (avec ou sans 0x) -> octets. Retourne false si la forme est invalide.
bool fromHex(const std::string& s, bytes& out);

// ---------------------------------------------------------------
// u256 — entier 256 bits non signé, 4 mots de 64 bits (little-endian),
// avec une arithmétique modulaire de Montgomery pour les corps de courbe.
// ---------------------------------------------------------------
class u256 {
public:
    std::array<uint64_t, 4> l{0, 0, 0, 0};

    u256() = default;
    u256(uint64_t v) { l[0] = v; }

    // Analyse depuis hex (avec ou sans 0x) ou décimale.
    static u256 fromHex(const std::string& s);
    static u256 fromDec(const std::string& s);
    static fix32 toBytes(const u256& v); // big-endian, 32 octets
    static u256 fromBytes(const fix32& b);
    static u256 fromBytes(const uint8_t* p, size_t n); // big-endian

    std::string toHexStr() const;
    std::string toDecStr() const;

    bool isZero() const { return !(l[0] | l[1] | l[2] | l[3]); }

    int cmp(const u256& o) const;
    bool operator==(const u256& o) const { return cmp(o) == 0; }
    bool operator!=(const u256& o) const { return cmp(o) != 0; }
    bool operator<(const u256& o) const { return cmp(o) < 0; }
    bool operator>(const u256& o) const { return cmp(o) > 0; }
    bool operator<=(const u256& o) const { return cmp(o) <= 0; }
    bool operator>=(const u256& o) const { return cmp(o) >= 0; }

    u256& addMod(const u256& b, const u256& mod);       // addition (classique, sans retenue au-delà de 256 bits)
    u256& subMod(const u256& b, const u256& mod);       // soustraction
    u256& shlBits(int n);
    u256& shrBits(int n);
    int bit(int i) const;

    // Arithmétique modulaire (Montgomery). Valeurs interprétées en forme Montgomery.
    static u256 toMont(const u256& a, const u256& mod);
    static u256 fromMont(const u256& a, const u256& mod);
    static u256 montMul(const u256& a, const u256& b, const u256& mod);
    static u256 powMod(const u256& a, const u256& e, const u256& mod);
    static u256 invMod(const u256& a, const u256& mod);
    static u256 mod(const u256& a, const u256& mod);
    static void divmod(const u256& a, const u256& b, u256& q, u256& r);

    // produit exact sur 512 bits (pour mémoire), réduit par le modulo ;
    static u256 mulMod(const u256& a, const u256& b, const u256& mod);

    static u256 one() { return u256(1); }
    static u256 max() { u256 v; for (auto& x : v.l) x = ~uint64_t(0); return v; }
};

// ---------------------------------------------------------------
// Aléa cryptographique
// ---------------------------------------------------------------
bytes randomBytes(size_t n);

} // namespace nstrike