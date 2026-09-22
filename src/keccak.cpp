// ------------------------------------------------------------------
// Nstrike — Keccak-f[1600] et Keccak-256 / SHA3-256. Permutation validée
// contre le vecteur XKCP (perm(0) = f1258f7940e1dde7...).
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#include "keccak.hpp"

#include <cstring>

namespace nstrike {

static const size_t KECCAK_RATE = 136;

static inline uint64_t ROL(uint64_t x, int n) { return (x << n) | (x >> (64 - n)); }

static const uint8_t RHO[5][5] = {
    {0, 36, 3, 41, 18},
    {1, 44, 10, 45, 2},
    {62, 6, 43, 15, 61},
    {28, 55, 25, 21, 56},
    {27, 20, 39, 8, 14}};

static const uint64_t RC[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL, 0x8000000080008000ULL,
    0x000000000000808bULL, 0x0000000080000001ULL, 0x8000000080008081ULL, 0x8000000000008009ULL,
    0x000000000000008aULL, 0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL, 0x8000000000008003ULL,
    0x8000000000008002ULL, 0x8000000000000080ULL, 0x000000000000800aULL, 0x800000008000000aULL,
    0x8000000080008081ULL, 0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL};

void keccak_f1600(uint64_t st[25]) {
    uint64_t C[5], B[5][5];
    for (int round = 0; round < 24; ++round) {
        for (int x = 0; x < 5; ++x)
            C[x] = st[x] ^ st[x + 5] ^ st[x + 10] ^ st[x + 15] ^ st[x + 20];
        for (int x = 0; x < 5; ++x) {
            uint64_t D = C[(x + 4) % 5] ^ ROL(C[(x + 1) % 5], 1);
            for (int y = 0; y < 5; ++y) st[x + 5 * y] ^= D;
        }
        for (int x = 0; x < 5; ++x)
            for (int y = 0; y < 5; ++y)
                B[y][(2 * x + 3 * y) % 5] = ROL(st[x + 5 * y], RHO[x][y]);
        // st[x + 5*y] = B[x][y] (indexation stricte ; un memcpy transposerait l'état)
        for (int x = 0; x < 5; ++x)
            for (int y = 0; y < 5; ++y)
                st[x + 5 * y] = B[x][y];
        // chi (copie de ligne : une écriture en place corromprait les lectures suivantes)
        uint64_t temp[5];
        for (int y = 0; y < 5; ++y) {
            for (int x = 0; x < 5; ++x) temp[x] = st[x + 5 * y];
            for (int x = 0; x < 5; ++x)
                st[x + 5 * y] = temp[x] ^ ((~temp[(x + 1) % 5]) & temp[(x + 2) % 5]);
        }
        st[0] ^= RC[round];
    }
}

void Keccak256::absorb(const uint8_t* p) {
    uint8_t* st = reinterpret_cast<uint8_t*>(st_);
    for (size_t i = 0; i < KECCAK_RATE; ++i) st[i] ^= p[i];
    keccak_f1600(st_);
}

void Keccak256::update(const uint8_t* data, size_t n) {
    size_t off = 0;
    while (off < n) {
        size_t take = KECCAK_RATE - idx_;
        if (take > n - off) take = n - off;
        uint8_t* st = reinterpret_cast<uint8_t*>(st_);
        for (size_t k = 0; k < take; ++k) st[idx_ + k] ^= data[off + k];
        idx_ += take;
        off += take;
        if (idx_ == KECCAK_RATE) { keccak_f1600(st_); idx_ = 0; }
    }
}

fix32 Keccak256::digest() {
    uint8_t* st = reinterpret_cast<uint8_t*>(st_);
    if (idx_ == KECCAK_RATE - 1) {
        st[idx_] ^= (uint8_t)(dom_ | 0x80); // suffixe + bit de fin au même octet
        keccak_f1600(st_);
        idx_ = 0;
    } else {
        st[idx_] ^= dom_;
        st[KECCAK_RATE - 1] ^= 0x80;
    }
    keccak_f1600(st_);
    fix32 out;
    std::memcpy(out.data(), st, 32);
    return out;
}

fix32 Keccak256::hash(const uint8_t* data, size_t n, uint8_t dom) {
    Keccak256 k(dom);
    k.update(data, n);
    return k.digest();
}

} // namespace nstrike