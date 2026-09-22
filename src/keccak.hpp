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

// Keccak-256 (le Keccak utilisé par Ethereum pour les adresses), from scratch.
class Keccak256 {
public:
    // dom : suffixe de padding — 0x01 pour Keccak-256 (Ethereum), 0x06 pour SHA3-256.
    explicit Keccak256(uint8_t dom = 0x01) : dom_(dom) {}
    void update(const uint8_t* data, size_t n);
    void update(const bytes& b) { update(b.data(), b.size()); }
    fix32 digest();

    static fix32 hash(const uint8_t* data, size_t n, uint8_t dom = 0x01);
    static fix32 hash(const bytes& b, uint8_t dom = 0x01) { return hash(b.data(), b.size(), dom); }

private:
    uint64_t st_[25] = {0};
    size_t idx_ = 0;
    uint8_t dom_ = 0x01;
    void absorb(const uint8_t* p);
};

// Keccak-f[1600] exposée (tests, PoW optionnel sur le cœr de Keccak).
void keccak_f1600(uint64_t st[25]);

inline fix32 keccak256(const bytes& b) { return Keccak256::hash(b); }

} // namespace nstrike