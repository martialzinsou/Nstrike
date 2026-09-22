// ------------------------------------------------------------------
// Nstrike — harnais de tests (validations des primitives et de la chaîne).
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "common.hpp"
#include "crypto.hpp"
#include "keccak.hpp"
#include "sha256.hpp"

using namespace nstrike;

// ---------------------------------------------------------------
// Mini-harnais de tests
// ---------------------------------------------------------------
struct TestCase {
    const char* name;
    void (*fn)();
};

static std::vector<TestCase>& tests() {
    static std::vector<TestCase> t;
    return t;
}

static int g_fail = 0;
static int g_check = 0;

struct TestRegistrar {
    TestRegistrar(const char* n, void (*f)()) { tests().push_back({n, f}); }
};

#define TEST(name)                       \
    static void test_##name();           \
    static TestRegistrar reg_##name(#name, test_##name); \
    static void test_##name()

#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)
#define CHECK(cond)                                                        \
    do {                                                                   \
        ++g_check;                                                         \
        if (!(cond)) {                                                     \
            ++g_fail;                                                      \
            std::fprintf(stderr, "  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        }                                                                  \
    } while (0)

#define CHECK_MEMEQ(a, b, n) CHECK(std::memcmp(a, b, n) == 0)

#define CHECK_STR(s, expected)                 \
    do {                                       \
        std::string got_ = (s);                \
        if (got_ != (expected)) {              \
            ++g_fail; ++g_check;               \
            std::fprintf(stderr, "  FAIL %s:%d (%s) -> \"%s\" != \"%s\"\n", \
                         __FILE__, __LINE__, #s, got_.c_str(), expected);  \
        } else ++g_check;                      \
    } while (0)

static bool hexEq(const fix32& a, const std::string& hex) {
    return toHex(a) == hex;
}
#define CHECK_HASH(h, hex) CHECK(hexEq(h, hex))

// ---------------------------------------------------------------
// SHA-256
// ---------------------------------------------------------------
TEST(sha256_vectors) {
    CHECK_HASH(sha256(bytes{}), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK_HASH(sha256(bytes{'a', 'b', 'c'}), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    std::string msg;
    for (int i = 0; i < 1000; ++i) msg += (char)('a' + (i % 26));
    fix32 h = SHA256::hash(reinterpret_cast<const uint8_t*>(msg.data()), msg.size());
    // Vérifie que le hachage par blocs concorde avec celui par morceaux.
    SHA256 s;
    for (size_t i = 0; i < msg.size(); i += 7) s.update(reinterpret_cast<const uint8_t*>(msg.data() + i), std::min<size_t>(7, msg.size() - i));
    CHECK_MEMEQ(s.digest().data(), h.data(), 32);
}

// ---------------------------------------------------------------
// Keccak-256
// ---------------------------------------------------------------
TEST(keccak256_vectors) {
    CHECK_HASH(keccak256(bytes{}), "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470");
    CHECK_HASH(keccak256(bytes{'a', 'b', 'c'}), "4e03657aea45a94fc7d47ba826c8d667c0d1e6e33a64a036ec44f58fa12d6c45");

    bytes data;
    for (int i = 0; i < 300; ++i) data.push_back((uint8_t)(i * 7));
    fix32 h = keccak256(data);
    Keccak256 k;
    for (size_t i = 0; i < data.size(); i += 5)
        k.update(data.data() + i, std::min<size_t>(5, data.size() - i));
    CHECK_MEMEQ(k.digest().data(), h.data(), 32);
}

// ---------------------------------------------------------------
// u256
// ---------------------------------------------------------------
TEST(u256_basics) {
    CHECK(u256(5).toDecStr() == "5");
    CHECK(u256(0).toDecStr() == "0");
    u256 big = u256::fromDec("115792089237316195423570985008687907853269984665640564039457584007913129639935");
    CHECK(big.toDecStr() == "115792089237316195423570985008687907853269984665640564039457584007913129639935");
    u256 half = u256::fromHex("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
    CHECK(half.toDecStr() == "57896044618658097711785492504343953926634992332820282019728792003956564819967");
}

TEST(u256_modular) {
    const u256 p = secp256k1P();
    u256 a = u256::fromDec("1234567890123456789012345678901234567890123456789");
    u256 inv = u256::invMod(a, p);
    u256 prod = u256::mulMod(a, inv, p);
    CHECK(prod == u256::one());

    u256 g = u256::fromDec("987654321");
    u256 e1 = u256::fromDec("12345");
    u256 e2 = u256::fromDec("67890");
    u256 r1 = u256::powMod(g, e1, p);
    u256 r2 = u256::powMod(g, e2, p);
    u256 r3 = u256::powMod(g, u256(e1.l[0] + e2.l[0]), p);
    CHECK(u256::mulMod(r1, r2, p) == r3);
}

// ---------------------------------------------------------------
// secp256k1 — points & ECDSA
// ---------------------------------------------------------------
TEST(ecc_basics) {
    PublicKey G = ecMulG(u256::one());
    CHECK(G.x == secp256k1Gx());
    CHECK(G.y == secp256k1Gy());

    u256 k1 = u256::fromDec("32556151");
    u256 k2 = u256::fromDec("123456789987654321");
    u256 ksum(k1.l[0] + k2.l[0]);
    PublicKey P1 = ecMulG(k1);
    PublicKey P2 = ecMulG(k2);
    PublicKey Psum = ecAdd(P1, P2);
    PublicKey Pdirect = ecMulG(ksum);
    CHECK(Psum.x == Pdirect.x);
    CHECK(Psum.y == Pdirect.y);

    PublicKey negP1;
    negP1.x = P1.x;
    negP1.y = secp256k1P();
    negP1.y.subMod(P1.y, secp256k1P());
    PublicKey inf = ecAdd(P1, negP1);
    CHECK(ecIsInfinity(inf));
}

TEST(ecdsa_roundtrip) {
    bytes priv = generatePrivate();
    PublicKey pub = pubFromPrivate(priv);
    fix20 addr = addressFromPrivate(priv);

    bytes msg = bytes{'N', 's', 't', 'r', 'i', 'k', 'e', '!'};
    fix32 h = keccak256(msg);

    Signature sig = ecdsaSign(priv, h);
    CHECK(sig.r[0] != 0xff || sig.s[0] != 0xff);
    CHECK(ecdsaVerify(h, sig, pub));

    fix32 bad = h;
    bad[0] ^= 1;
    CHECK(!ecdsaVerify(bad, sig, pub));

    PublicKey rec = ecdsaRecover(h, sig);
    CHECK(addressFromPublicKey(rec) == addr);
    CHECK(ecdsaVerify(h, sig, rec));
}

TEST(ecdsa_many) {
    for (int i = 0; i < 5; ++i) {
        bytes priv = generatePrivate();
        bytes msg = randomBytes(32);
        fix32 h = keccak256(msg);
        Signature sig = ecdsaSign(priv, h);
        PublicKey rec = ecdsaRecover(h, sig);
        CHECK(addressFromPublicKey(rec) == addressFromPrivate(priv));
    }
}

// ---------------------------------------------------------------
// Divers
// ---------------------------------------------------------------
TEST(hex_roundtrip) {
    bytes b = randomBytes(33);
    std::string h = "0x" + toHex(b);
    bytes back;
    CHECK(fromHex(h, back));
    CHECK(back == b);
    bytes bad;
    CHECK(!fromHex("zz", bad));
    CHECK(fromHex("0", bad)); // impair -> préfixé d'un zéro
    CHECK(bad.size() == 1 && bad[0] == 0x00);
}

int main(int argc, char** argv) {
    std::string filter = (argc > 1 && std::string(argv[1]) == "--all") ? "" : (argc > 1 ? argv[1] : "");
    int run = 0;
    for (auto& t : tests()) {
        if (!filter.empty() && std::string(t.name).find(filter) == std::string::npos) continue;
        ++run;
        int before = g_fail;
        t.fn();
        if (before == g_fail)
            std::printf("[ OK ] %s\n", t.name);
        else
            std::printf("[FAIL] %s\n", t.name);
    }
    std::printf("\n%d test(s), %d vérification(s), %d échec(s)\n", run, g_check, g_fail);
    return g_fail ? 1 : 0;
}