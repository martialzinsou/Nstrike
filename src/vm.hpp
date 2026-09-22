// ------------------------------------------------------------------
// Nstrike — mini-VM « NVM » : pile de mots 32 octets, assembleur et
// template de jeton type ERC-20. Jeu d'opcodes documenté dans vm.cpp.
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#pragma once

#include <string>

#include "common.hpp"
#include "state.hpp"

namespace nstrike {

// ---------------------------------------------------------------
// Mini-VM NVM : pile de mots 32 octets (grand-boutiste), style EVM.
// Jeu d'opcodes documenté dans vm.cpp. SLOAD/SSTORE/KECCAK/calldata
// pensés pour les contrats type ERC-20 sans mémoire linéaire.
// ---------------------------------------------------------------

struct CallContext {
    fix20 self;      // adresse du contrat
    fix20 origin;    // expéditeur de la transaction (EOA)
    fix20 caller;    // appelant immédiat
    u256 callvalue;  // valeur transférée avec l'appel
    bytes calldata;  // selector(4) + arguments 32 octets (ABI simplifié)
};

struct VMResult {
    bool ok = false;
    uint64_t gasUsed = 0;
    bytes output;    // mots produits par RESULT(n), dans l'ordre du pop
};

// Services fournis par la VM pour l'exécution (injectés par le runneur).
struct VMIO {
    State& state;
    uint64_t* logs = nullptr; // éventuel (non utilisé par le template)
};

// Exécute le bytecode du contrat `self` avec le contexte donné.
// Mutations de storage écrites directement dans `state`.
VMResult vmExecute(State& state, const CallContext& ctx, uint64_t gas);

// ---------------------------------------------------------------
// Assembleur NVM (mini langage) -> bytecode.
//  - `L_x:`   définit un label (émet JUMPDEST)
//  - `PUSH1..PUSH32 <hex|dec>` ; `PUSH_SEL "balanceOf(address)"`
//  - `JUMP L_x` / `JUMPI L_x`  (adresses de label émises en PUSH2)
//  - opcodes: STOP ADD SUB MUL DIV MOD LT GT EQ ISZERO AND OR XOR NOT
//             SHL SHR CALLDATASIZE CALLDATALOAD CALLER ORIGIN ADDRESS
//             CALLVALUE SLOAD SSTORE KECCAK RESULT REVERT JUMP JUMPI
//             JUMPDEST POP DUP1..16 SWAP1..16
//  - commentaires `//`.
bytes assembleNvm(const std::string& src);

// Selecteur ABI simplifié : premiers 4 octets de keccak256(signature).
fix32 abiSelector(const std::string& sig);

// Extrait le mot (32 octets) i de la sortie d'un appel.
fix32 resultWord(const bytes& output, size_t i = 0);

// ---------------------------------------------------------------
// Template de jeton "ERC-20" compilé en NVM.
// ---------------------------------------------------------------
bytes erc20Bytecode();
// Déploie le jeton pour le compte `creator` : stocke le code, mint la
// dotation initiale au créateur, fixe nom/symbole/décimales.
fix20 erc20Deploy(State& state, const fix20& creator, const u256& creatorNonce,
                  const std::string& name, const std::string& symbol,
                  uint8_t decimals, const u256& initialSupply);

} // namespace nstrike