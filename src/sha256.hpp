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

// SHA-256 implémenté from scratch (aucune dépendance externe).
class SHA256 {
public:
    SHA256();
    void update(const uint8_t* data, size_t n);
    void update(const bytes& b) { update(b.data(), b.size()); }
    fix32 digest();

    static fix32 hash(const uint8_t* data, size_t n);
    static fix32 hash(const bytes& b) { return hash(b.data(), b.size()); }

private:
    uint32_t h_[8];
    uint64_t total_ = 0;
    uint8_t buf_[64];
    size_t buflen_ = 0;
    void processBlock(const uint8_t* p);
};

inline fix32 sha256(const bytes& b) { return SHA256::hash(b); }

} // namespace nstrike