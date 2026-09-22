// ------------------------------------------------------------------
// Nstrike — mini-VM « NVM » :
//   * interpréteur à pile de mots 32 octets (grand-boutiste, style EVM),
//   * assembleur (jetons + labels + PUSH_SEL),
//   * template de jeton type ERC-20 compilé en NVM.
// Jeu d'opcodes documenté ci-dessous. Storage via State (SLOAD/SSTORE),
// calldata d'appel et hachage Keccak-256 pour les clés de mapping.
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#include "vm.hpp"

#include <cctype>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

#include "keccak.hpp"

namespace nstrike {

// ---------------------------------------------------------------
// Opcodes NVM — numérotation inspirée de l'EVM ; extensions documentées :
//   KECCAK  (0x20) pop b, pop a -> push keccak256(a32||b32)  [clé mapping]
//   RESULT  (0x3F) pop n -> sortie = les n mots du sommet (ordre du pop)
//   REVERT  (0xFD) pop offset, pop size -> échec de l'appel
// ---------------------------------------------------------------
enum : uint8_t {
    NOP = 0x00,       STOP = 0x00,
    ADD = 0x01,       // (a+b) mod 2^256
    MUL = 0x02,       // (a*b) mod 2^256 (troncature des 256 bits hauts)
    SUB = 0x03,       // a-b mod 2^256
    DIV = 0x04,       // a/b (0 si b==0)
    MOD = 0x06,       // a%b (0 si b==0)
    LT = 0x10,        // (a < b)  avec a = sommet de la pile
    GT = 0x11,        // (a > b)
    EQ = 0x14,        // (a == b)
    ISZERO = 0x15,    // (a == 0)
    AND = 0x16, OR = 0x17, XOR = 0x18, NOT = 0x19,
    SHL = 0x1B,       // (a<<b) avec a = décalage, b = valeur (EVM : a<<b)
    SHR = 0x1C,
    KECCAK = 0x20,    // voir en-tête
    ADDRESS = 0x30,   // push self
    CALLER = 0x33,    // push caller (appelant immédiat)
    CALLVALUE = 0x34, // push callvalue
    CALLDATALOAD = 0x35, // pop offset -> push calldata[offset..offset+32]
    CALLDATASIZE = 0x36, // push taille du calldata
    RESULT = 0x3F,    // voir en-tête
    POP = 0x50,
    SLOAD = 0x54,     // pop key -> push storage[self][key]
    SSTORE = 0x55,    // pop value, pop key -> storage[self][key] = value
    JUMP = 0x56,      // pop dest (doit pointer sur JUMPDEST)
    JUMPI = 0x57,     // pop dest, pop cond -> saute si cond != 0
    JUMPDEST = 0x5B,  // cible de saut
    PUSH1 = 0x60,     // .. PUSH32 = 0x7F : push la valeur immédiate
    DUP1 = 0x80,      // .. DUP16 = 0x8F : duplique le n-ième élément
    SWAP1 = 0x90,     // .. SWAP16 = 0x9F : échange les deux éléments du sommet
    REVERT = 0xFD,    // voir en-tête
    INVALID = 0xFE,   // échec immédiat
};

// ---------------------------------------------------------------
// Mots 32 octets & aide pile
// ---------------------------------------------------------------
namespace {
using Word = fix32;

Word toWord(const u256& v) { return u256::toBytes(v); }          // big-endian
Word toWord64(uint64_t v) {                                       // right-aligné
    Word w{};
    for (int i = 0; i < 8; ++i) w[31 - i] = uint8_t(v >> (8 * i));
    return w;
}
u256 fromWord(const Word& w) { return u256::fromBytes(w); }
bool wordNonZero(const Word& w) { return !fromWord(w).isZero(); }

struct VM {
    State& state;           // storage mutable pendant l'exécution
    const bytes& code;      // bytecode du contrat
    CallContext ctx;
    uint64_t gas = 0;       // gas restant (décrété à chaque opcode)
    std::vector<Word> stack;
    bytes output;           // résultat RESULT
    bool failed = false;

    VM(State& s, const bytes& c, const CallContext& cc, uint64_t g)
        : state(s), code(c), ctx(cc), gas(g) {}

    // Prélève du gas ; échec (out-of-gas) si insuffisant.
    void charge(uint64_t g) {
        if (g > gas) failed = true;
        else gas -= g;
    }
    Word pop() {
        if (stack.empty()) { failed = true; return Word{}; }
        Word w = stack.back();
        stack.pop_back();
        return w;
    }
    void push(const Word& w) { if (stack.size() < 1024) stack.push_back(w); }
};

// Barème de gas simplifié (MVP) : constantes par opcode, SSTORE très cher.
uint64_t gasOf(const bytes& code, size_t pc) {
    uint8_t op = code[pc];
    if (op >= PUSH1 && op <= PUSH1 + 31) return 3;
    switch (op) {
        case JUMPDEST: case NOP: return 1;
        case SLOAD: return 100;
        case KECCAK: return 30;
        case SSTORE: return 20000;
        default: return 3;
    }
}

// produit a*b tronqué aux 256 bits bas — colonnes à retenue propagée,
// même schéma fiable que le REDC de common.cpp.
u256 mulLow(const u256& a, const u256& b) {
    uint64_t t[9] = {0};
    for (int i = 0; i < 4; ++i) {
        uint64_t carry = 0;
        for (int j = 0; j < 4; ++j) {
            __uint128_t cur = (__uint128_t)a.l[i] * b.l[j] + t[i + j] + carry;
            t[i + j] = (uint64_t)cur;
            carry = (uint64_t)(cur >> 64);
        }
        int idx = i + 4; // report dans les mots hauts (t[8] au plus)
        while (carry) {
            __uint128_t s = (__uint128_t)t[idx] + carry;
            t[idx] = (uint64_t)s;
            carry = (uint64_t)(s >> 64);
            ++idx;
        }
    }
    u256 r;
    r.l = {t[0], t[1], t[2], t[3]};
    return r;
}
} // namespace

// ---------------------------------------------------------------
// Interpréteur
// ---------------------------------------------------------------
VMResult vmExecute(State& state, const CallContext& ctx, uint64_t gas) {
    const bytes& code = state.code(ctx.self);
    VMResult res;
    // Appel sans code : succès neutre (virements simples vers un EOA).
    if (code.empty()) { res.ok = true; return res; }

    VM vm(state, code, ctx, gas);
    size_t pc = 0;
    while (pc < code.size() && !vm.failed) {
        uint64_t g = gasOf(code, pc);
        vm.charge(g);
        if (vm.failed) break;
        uint8_t op = code[pc];

        // Instructions à données immédiates
        if (op >= PUSH1 && op <= PUSH1 + 31) {
            size_t n = op - PUSH1 + 1;
            if (pc + 1 + n > code.size()) { vm.failed = true; break; }
            Word w{};
            std::copy(code.begin() + pc + 1, code.begin() + pc + 1 + n, w.end() - n);
            vm.push(w);
            pc += 1 + n;
            continue;
        }

        switch (op) {
            case STOP: {
                res.ok = true;
                res.gasUsed = vm.gas;
                res.output = std::move(vm.output);
                return res;
            }
            case ADD: {
                u256 r;
                bool carry = false;
                for (int i = 0; i < 4; ++i) {
                    __uint128_t t = (__uint128_t)fromWord(vm.pop()).l[i]   // pop a (haut)
                                  + fromWord(vm.pop()).l[i]                // pop b
                                  + (carry ? 1 : 0);
                    r.l[i] = (uint64_t)t;
                    carry = t >> 64;
                }
                vm.push(toWord(r));
                break;
            }
            case MUL: {
                Word a = vm.pop(), b = vm.pop();
                vm.push(toWord(mulLow(fromWord(a), fromWord(b))));
                break;
            }
            case SUB: {
                Word a = vm.pop(), b = vm.pop(); // a = sommet : (a-b) mod 2^256
                u256 r;
                bool borrow = false;
                for (int i = 0; i < 4; ++i) {
                    __uint128_t t = (__uint128_t)fromWord(a).l[i] - fromWord(b).l[i] - (borrow ? 1 : 0);
                    r.l[i] = (uint64_t)t;
                    borrow = (t >> 64) != 0;
                }
                vm.push(toWord(r));
                break;
            }
            case DIV: case MOD: {
                Word a = vm.pop(), b = vm.pop();
                u256 x = fromWord(a), d = fromWord(b);
                if (d.isZero()) vm.push(Word{});
                else {
                    u256 q, r;
                    u256::divmod(x, d, q, r);
                    vm.push(toWord(op == DIV ? q : r));
                }
                break;
            }
            case LT: case GT: case EQ: {
                Word a = vm.pop(), b = vm.pop(); // a = sommet
                bool t = op == LT ? fromWord(a) < fromWord(b)
                       : op == GT ? fromWord(a) > fromWord(b)
                       : a == b;
                vm.push(toWord64(t ? 1 : 0));
                break;
            }
            case ISZERO: {
                Word a = vm.pop();
                vm.push(toWord64(wordNonZero(a) ? 0 : 1));
                break;
            }
            case AND: case OR: case XOR: {
                Word a = vm.pop(), b = vm.pop();
                Word r{};
                for (int i = 0; i < 32; ++i)
                    r[i] = op == AND ? (a[i] & b[i]) : op == OR ? (a[i] | b[i]) : (a[i] ^ b[i]);
                vm.push(r);
                break;
            }
            case NOT: {
                Word a = vm.pop();
                for (auto& c : a) c = ~c;
                vm.push(a);
                break;
            }
            case SHL: case SHR: {
                Word a = vm.pop(), b = vm.pop(); // a = décalage, b = valeur
                uint64_t sh = fromWord(a).l[0];
                u256 v = fromWord(b);
                if (sh > 255) { vm.push(Word{}); break; }
                if (op == SHL) { v.shlBits((int)sh); } else { v.shrBits((int)sh); }
                vm.push(toWord(v));
                break;
            }
            case KECCAK: {
                Word b = vm.pop(), a = vm.pop(); // hash(a||b)
                bytes in;
                in.reserve(64);
                in.insert(in.end(), a.begin(), a.end());
                in.insert(in.end(), b.begin(), b.end());
                vm.push(Keccak256::hash(in));
                break;
            }
            case ADDRESS:   vm.push(toWord(u256::fromBytes(ctx.self.data(), 20))); break;
            case CALLER:    vm.push(toWord(u256::fromBytes(ctx.caller.data(), 20))); break;
            case CALLVALUE: vm.push(toWord(ctx.callvalue)); break;
            case CALLDATASIZE: vm.push(toWord64((uint64_t)ctx.calldata.size())); break;
            case CALLDATALOAD: {
                uint64_t o = fromWord(vm.pop()).l[0];
                Word w{};
                for (size_t i = 0; i < 32; ++i)
                    if (o + i < ctx.calldata.size()) w[i] = ctx.calldata[o + i];
                vm.push(w);
                break;
            }
            case RESULT: {
                uint64_t n = fromWord(vm.pop()).l[0];
                if (n > vm.stack.size()) { vm.failed = true; break; }
                vm.output.clear();
                for (uint64_t i = 0; i < n; ++i) {
                    const Word& w = vm.stack[vm.stack.size() - 1 - i]; // exprès : sommet d'abord
                    vm.output.insert(vm.output.end(), w.begin(), w.end());
                }
                vm.stack.erase(vm.stack.end() - n, vm.stack.end());
                break;
            }
            case POP: vm.pop(); break;
            case JUMP: case JUMPI: {
                uint64_t d = fromWord(vm.pop()).l[0]; // dest = sommet
                if (op == JUMPI) {
                    Word cond = vm.pop();
                    if (!wordNonZero(cond)) break;
                }
                if (d >= code.size() || code[d] != JUMPDEST) { vm.failed = true; break; }
                pc = (size_t)d;
                continue;
            }
            case JUMPDEST: break;
            case SLOAD: {
                Word k = vm.pop();
                vm.push(toWord(state.getStorage(ctx.self, k)));
                break;
            }
            case SSTORE: {
                Word v = vm.pop();      // valeur = sommet
                Word k = vm.pop();      // puis clé
                state.setStorage(ctx.self, k, fromWord(v));
                break;
            }
            case REVERT: {
                vm.pop(); vm.pop();
                vm.failed = true;
                break;
            }
            case INVALID: default: vm.failed = true; break;
        }
        ++pc;
    }

    res.ok = !vm.failed;
    res.gasUsed = vm.gas;
    res.output = std::move(vm.output);
    return res;
}

// ---------------------------------------------------------------
// Assembleur NVM
//   Syntaxe :
//     L_x:            label -> JUMPDEST
//     PUSH1..PUSH32 <hex|dec>   (taille minimale effective)
//     PUSH2 <L_x>     adresse de label -> PUSH2 (rarement > 16 bits)
//     PUSH_SEL "sig"  selector = premiers 4 octets de keccak256(sig),
//                     alignés à gauche dans un mot 32 octets (même encodage
//                     que CALLDATALOAD(0) côté EVM)
//     JUMP L_x / JUMPI L_x  équivalents (L_x auto-émis en PUSH2)
//   Deux passes : adresses des labels puis émission.
// ---------------------------------------------------------------
fix32 abiSelector(const std::string& sig) {
    return Keccak256::hash(bytes(sig.begin(), sig.end()));
}

fix32 resultWord(const bytes& output, size_t i) {
    fix32 w{};
    size_t at = i * 32;
    if (at + 32 <= output.size()) std::copy(output.begin() + at, output.begin() + at + 32, w.begin());
    return w;
}

namespace {
struct Inst { std::string op, arg; };

std::vector<std::string> tokenize(const std::string& src) {
    std::vector<std::string> toks;
    std::istringstream in(src);
    std::string tok;
    while (in >> tok) {
        // découper les commentaires de ligne
        size_t c = tok.find("//");
        if (c != std::string::npos) break;
        toks.push_back(tok);
    }
    return toks;
}

bool isPush(const std::string& op) {
    return op.rfind("PUSH", 0) == 0 && op.size() > 4;
}

uint64_t parseNum(const std::string& s) {
    if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0)
        return std::strtoull(s.substr(2).c_str(), nullptr, 16);
    return (uint64_t)std::strtoull(s.c_str(), nullptr, 10);
}

// Nombre d'octets de la valeur immédiate encodée (taille minimale).
size_t immBytes(const std::string& s) {
    if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) {
        std::string h = s.substr(2);
        if (h.empty()) h = "0";
        if (h.size() % 2) h = "0" + h;
        return h.size() / 2;
    }
    uint64_t v = (uint64_t)std::strtoull(s.c_str(), nullptr, 10);
    if (v == 0) return 1;
    size_t n = 0;
    while (v) { ++n; v >>= 8; }
    return n;
}

// Émet la taille en octets de l'instruction (hors labels : 1 octet).
size_t instSize(const Inst& in) {
    if (!in.op.empty() && in.op.back() == ':') return 1;     // JUMPDEST
    if (in.op == "PUSH_SEL") return 1 + 32;
    if (isPush(in.op)) {
        if (!in.arg.empty() && in.arg[0] == 'L') return 3;   // PUSH2 label
        size_t nb = immBytes(in.arg);
        return 1 + std::min<size_t>(nb, 32);
    }
    return 1;
}
} // namespace

bytes assembleNvm(const std::string& src) {
    std::vector<std::string> toks = tokenize(src);

    // Phase 1 : liste d'instructions (un opcode contraint son argument).
    std::vector<Inst> insts;
    for (size_t i = 0; i < toks.size(); ++i) {
        Inst in;
        in.op = toks[i];
        if (isPush(in.op) || in.op == "PUSH_SEL") {
            if (i + 1 < toks.size() && toks[i + 1].back() != ':') { in.arg = toks[i + 1]; ++i; }
        }
        insts.push_back(in);
    }

    // Phase 2 : adresse (en octets) de chaque label.
    std::map<std::string, uint16_t> labels;
    {
        size_t at = 0;
        for (const Inst& in : insts) {
            if (!in.op.empty() && in.op.back() == ':') {
                std::string name = in.op;
                name.pop_back();
                labels[name] = (uint16_t)at;
            }
            at += instSize(in);
        }
    }

    // Phase 3 : émission.
    bytes code;
    for (const Inst& in : insts) {
        if (!in.op.empty() && in.op.back() == ':') { code.push_back(JUMPDEST); continue; }
        if (in.op == "PUSH_SEL") {
            code.push_back(PUSH1 + 31);
            fix32 sel = abiSelector(in.arg);
            code.insert(code.end(), sel.begin(), sel.end());
            continue;
        }
        if (isPush(in.op)) {
            if (!in.arg.empty() && in.arg[0] == 'L') {
                // PUSH2 adresse de label (supporte toute instruction de saut)
                uint16_t ad = labels.count(in.arg) ? labels[in.arg] : 0;
                code.push_back(PUSH1 + 1);
                code.push_back(uint8_t(ad >> 8));
                code.push_back(uint8_t(ad & 0xFF));
                continue;
            }
            size_t nb = std::min<size_t>(immBytes(in.arg), 32);
            uint64_t v = parseNum(in.arg);
            code.push_back(PUSH1 + nb - 1);
            for (size_t k = 0; k < nb; ++k)
                code.push_back(uint8_t(v >> (8 * (nb - 1 - k))));
            continue;
        }

        // opcodes simples (table de correspondance nom -> octet)
        struct { const char* n; uint8_t op; } tbl[] = {
            {"STOP", STOP}, {"ADD", ADD}, {"MUL", MUL}, {"SUB", SUB}, {"DIV", DIV},
            {"MOD", MOD}, {"LT", LT}, {"GT", GT}, {"EQ", EQ}, {"ISZERO", ISZERO},
            {"AND", AND}, {"OR", OR}, {"XOR", XOR}, {"NOT", NOT}, {"SHL", SHL},
            {"SHR", SHR}, {"KECCAK", KECCAK}, {"ADDRESS", ADDRESS}, {"CALLER", CALLER},
            {"CALLVALUE", CALLVALUE}, {"CALLDATALOAD", CALLDATALOAD},
            {"CALLDATASIZE", CALLDATASIZE}, {"RESULT", RESULT}, {"POP", POP},
            {"SLOAD", SLOAD}, {"SSTORE", SSTORE}, {"JUMP", JUMP}, {"JUMPI", JUMPI},
            {"JUMPDEST", JUMPDEST}, {"REVERT", REVERT}, {"INVALID", INVALID},
            {"PUSH_SEL", NOP},
        };
        bool found = false;
        for (const auto& e : tbl) {
            if (in.op == e.n) { code.push_back(e.op); found = true; break; }
        }
        if (!found) {
            if (in.op.rfind("DUP", 0) == 0 && std::isdigit(in.op[3])) {
                int n = std::stoi(in.op.substr(3));
                if (n >= 1 && n <= 16) { code.push_back(DUP1 + n - 1); found = true; }
            } else if (in.op.rfind("SWAP", 0) == 0 && std::isdigit(in.op[4])) {
                int n = std::stoi(in.op.substr(4));
                if (n >= 1 && n <= 16) { code.push_back(SWAP1 + n - 1); found = true; }
            }
        }
        if (!found) throw std::runtime_error("assembleur NVM : instruction inconnue '" + in.op + "'");
    }
    return code;
}

// ---------------------------------------------------------------
// Template ERC-20 (NVM)
//   Layout du storage (clés = mots 32 octets des entiers de slot) :
//     slot 0 : totalSupply          slot 1 : name (ASCII gauche)
//     slot 2 : symbol               slot 3 : decimals
//     slot 4 : balances[addr]   = storage[keccak(addr||word(4))]
//     slot 5 : allowance            clé à deux niveaux :
//                inner = keccak(owner||word(5))
//                allowance[owner][spender] = storage[keccak(spender||inner)]
//   Appel : selector(4 octets) + arguments ABI (mots 32 octets).
// ---------------------------------------------------------------
const char* ERC20_SRC = R"NVM(
   // Garde : calldata minimum = selector.
   CALLDATASIZE PUSH1 4 LT PUSH2 L_revert JUMPI
   // Dispatch sur selector (word 0, left-aligned comme CALLDATALOAD(0)).
   PUSH32 0 CALLDATALOAD
   PUSH_SEL "totalSupply()"      EQ PUSH2 L_total    JUMPI
   PUSH_SEL "name()"             EQ PUSH2 L_name     JUMPI
   PUSH_SEL "symbol()"           EQ PUSH2 L_symbol   JUMPI
   PUSH_SEL "decimals()"         EQ PUSH2 L_decimals JUMPI
   PUSH_SEL "balanceOf(address)" EQ PUSH2 L_balance  JUMPI
   PUSH_SEL "transfer(address,uint256)"       EQ PUSH2 L_transfer JUMPI
   PUSH_SEL "approve(address,uint256)"        EQ PUSH2 L_approve  JUMPI
   PUSH_SEL "transferFrom(address,address,uint256)" EQ PUSH2 L_from JUMPI
   PUSH_SEL "allowance(address,address)"      EQ PUSH2 L_allow   JUMPI
   PUSH1 0 PUSH1 0 REVERT

L_total:
   PUSH1 0 SLOAD PUSH1 1 RESULT STOP
L_name:
   PUSH1 1 SLOAD PUSH1 1 RESULT STOP
L_symbol:
   PUSH1 2 SLOAD PUSH1 1 RESULT STOP
L_decimals:
   PUSH1 3 SLOAD PUSH1 1 RESULT STOP
L_balance:
   PUSH32 4 CALLDATALOAD
   PUSH1 4 KECCAK SLOAD
   PUSH1 1 RESULT STOP
L_transfer:
   // to = arg0 (offset 4), amount = arg1 (offset 36).
   PUSH32 4 CALLDATALOAD
   DUP1 ISZERO PUSH2 L_revert JUMPI            // refuser un virement vers 0
   CALLER PUSH1 4 KECCAK DUP1 SLOAD            // keyC, balC
   PUSH32 36 CALLDATALOAD                      // balC, amount
   DUP2 LT ISZERO PUSH2 L_revert JUMPI         // balC < amount -> revert
   SWAP1 SUB SSTORE                            // balC -= amount
   PUSH32 4 CALLDATALOAD PUSH1 4 KECCAK DUP1 SLOAD
   PUSH32 36 CALLDATALOAD ADD SSTORE           // bal(to) += amount
   PUSH1 1 RESULT STOP
L_approve:
   // innerC = keccak(caller||5), key = keccak(spender||innerC).
   CALLER PUSH1 5 KECCAK
   PUSH32 4 CALLDATALOAD SWAP1 KECCAK
   PUSH32 36 CALLDATALOAD SSTORE               // allowance = amount
   PUSH1 1 RESULT STOP
L_allow:
   // owner = arg0 (@4), spender = arg1 (@36).
   PUSH32 36 CALLDATALOAD
   PUSH32 4 CALLDATALOAD PUSH1 5 KECCAK
   KECCAK SLOAD
   PUSH1 1 RESULT STOP
L_from:
   // from = arg0 (@4), to = arg1 (@36), amount = arg2 (@68).
   PUSH32 4 CALLDATALOAD PUSH1 5 KECCAK        // innerF
   CALLER SWAP1 KECCAK DUP1 SLOAD              // keyA, allowed
   PUSH32 68 CALLDATALOAD
   DUP2 LT ISZERO PUSH2 L_revert JUMPI         // allowed < amount -> revert
   SWAP1 SUB SSTORE                            // allowed -= amount
   PUSH32 4 CALLDATALOAD PUSH1 4 KECCAK DUP1 SLOAD
   PUSH32 68 CALLDATALOAD
   DUP2 LT ISZERO PUSH2 L_revert JUMPI         // bal(from) < amount -> revert
   SWAP1 SUB SSTORE                            // bal(from) -= amount
   PUSH32 36 CALLDATALOAD PUSH1 4 KECCAK DUP1 SLOAD
   PUSH32 68 CALLDATALOAD ADD SSTORE           // bal(to) += amount
   PUSH1 1 RESULT STOP
L_revert:
   PUSH1 0 PUSH1 0 REVERT
)NVM";

bytes erc20Bytecode() { return assembleNvm(ERC20_SRC); }

// ---------------------------------------------------------------
// Déploiement d'un jeton type ERC-20
//   L'adresse du contrat dérive du créateur et de son nonce (avant
//   incrément du nonce par la transaction de création). Le créateur
//   reçoit la dotation initiale.
// ---------------------------------------------------------------
fix20 erc20Deploy(State& state, const fix20& creator, const u256& creatorNonce,
                  const std::string& name, const std::string& symbol,
                  uint8_t decimals, const u256& initialSupply) {
    fix20 addr = State::deriveContractAddress(creator, creatorNonce);
    state.setCode(addr, erc20Bytecode());

    // clé fix32 d'un slot entier
    auto slotKey = [](uint64_t s) { return u256::toBytes(u256(s)); };

    // nom / symbole : mot 32 octets alignés à gauche (ASCII)
    auto asciiWord = [](const std::string& s) {
        fix32 w{};
        for (size_t i = 0; i < s.size() && i < 32; ++i) w[i] = (uint8_t)s[i];
        return w;
    };

    // slota principaux
    state.setStorage(addr, slotKey(0), initialSupply);    // totalSupply
    state.setStorage(addr, slotKey(1), u256::fromBytes(asciiWord(name)));
    state.setStorage(addr, slotKey(2), u256::fromBytes(asciiWord(symbol)));
    state.setStorage(addr, slotKey(3), u256(decimals));

    // mint de la dotation au créateur : balances[creator] = initialSupply
    fix32 addrW = u256::toBytes(u256::fromBytes(creator.data(), creator.size()));
    fix32 slot4 = slotKey(4);
    bytes in;
    in.reserve(64);
    in.insert(in.end(), addrW.begin(), addrW.end());
    in.insert(in.end(), slot4.begin(), slot4.end());
    state.setStorage(addr, Keccak256::hash(in), initialSupply);

    return addr;
}

} // namespace nstrike