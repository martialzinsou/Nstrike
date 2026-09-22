/**
 * @file vm.hpp
 * @brief Mini-VM « NVM » : pile 32 octets, assembleur, template ERC-20
 * @author Martial Zinsou
 *
 * Machine virtuelle NVM (Nstrike Virtual Machine) — inspirée de l'EVM :
 * - Pile de mots 32 octets (fix32, big-endian logical, little-endian u256 interne)
 * - Maximum 1023 éléments (protection DoS)
 * - Pas de mémoire linéaire (simplification MVP) — accès direct calldata/storage
 * - Gas : coût par opcode (table simple, SLOAD=100, SSTORE=20000, autres=3)
 * - Jumps : JUMP/JUMPI vers JUMPDEST uniquement (validation statique)
 *
 * Jeu d'opcodes (numérotation EVM-like) :
 *   0x00 STOP         0x01 ADD          0x02 MUL          0x03 SUB
 *   0x04 DIV          0x06 MOD          0x10 LT           0x11 GT
 *   0x14 EQ           0x15 ISZERO       0x16 AND          0x17 OR
 *   0x18 XOR          0x19 NOT          0x1B SHL          0x1C SHR
 *   0x20 KECCAK       0x30 ADDRESS      0x33 CALLER       0x34 CALLVALUE
 *   0x35 CALLDATALOAD 0x36 CALLDATASIZE 0x3F RESULT       0x50 POP
 *   0x54 SLOAD        0x55 SSTORE       0x56 JUMP         0x57 JUMPI
 *   0x5B JUMPDEST     0x60..0x7F PUSH1..PUSH32
 *   0x80..0x8F DUP1..16 0x90..0x9F SWAP1..16
 *   0xFD REVERT        0xFE INVALID
 *
 * Extensions NVM :
 * - KECCAK (0x20) : pop b, pop a → push keccak256(a||b) [clé mapping]
 * - RESULT (0x3F) : pop n → output = n mots pop (ordre pop)
 * - REVERT (0xFD) : pop offset, pop size → échec appel
 * - Pas de CREATE/CALL (création via tx isCreate ; pas d'appel inter-contrat MVP)
 *
 * Assembleur NVM :
 * - Labels : `L_name:` → émet JUMPDEST
 * - PUSH1..PUSH32 <hex|dec> : taille minimale effective
 * - PUSH2 <L_label> : adresse label (PUSH2 auto par JUMP/JUMPI)
 * - PUSH_SEL "sig" : selector = keccak256(sig)[0..3] aligné gauche (32 octets)
 * - Commentaires `//`
 * - Deux passes : adresses labels → émission
 *
 * Template ERC-20 (compilé en NVM) :
 * - Slots : 0=totalSupply, 1=name, 2=symbol, 3=decimals, 4=balances, 5=allowance
 * - balances[addr] = storage[keccak(addr||word(4))]
 * - allowance[owner][spender] = storage[keccak(spender||keccak(owner||5))]
 * - Fonctions : totalSupply, name, symbol, decimals, balanceOf, transfer,
 *   approve, allowance, transferFrom (dispatch par selector)
 */

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
// Contexte d'appel VM
// ---------------------------------------------------------------
/**
 * @struct CallContext
 * @brief Contexte d'exécution d'un contrat (fourni par le runneur).
 * @param self      Adresse du contrat exécuté.
 * @param origin    Expéditeur de la transaction d'origine (EOA).
 * @param caller    Appelant immédiat (EOA ou contrat).
 * @param callvalue Valeur en nwei transférée avec l'appel.
 * @param calldata  Données d'appel : selector(4) + arguments ABI (mots 32 octets).
 */
struct CallContext {
    fix20 self;      ///< Adresse du contrat exécuté
    fix20 origin;    ///< Expéditeur de la transaction (EOA)
    fix20 caller;    ///< Appelant immédiat
    u256 callvalue;  ///< Valeur transférée (nwei)
    bytes calldata;  ///< Selector(4) + args 32 octets (ABI simplifié)
};

// ---------------------------------------------------------------
// Résultat d'exécution VM
// ---------------------------------------------------------------
/**
 * @struct VMResult
 * @brief Résultat d'exécution VM.
 * @param ok        true si exécution réussie (pas REVERT/INVALID/OOG).
 * @param gasUsed   Gas consommé (startGas - gasRestant).
 * @param output    Octets de sortie (mots 32 octets produits par RESULT).
 */
struct VMResult {
    bool ok = false;
    uint64_t gasUsed = 0;
    bytes output;    ///< Mots 32 octets concaténés (ordre = ordre du pop)
};

// ---------------------------------------------------------------
// Exécution VM
// ---------------------------------------------------------------
/**
 * @brief Exécute le bytecode du contrat `self` avec le contexte donné.
 * @param state  État global (muté pour SLOAD/SSTORE).
 * @param ctx    Contexte d'appel (self, origin, caller, callvalue, calldata).
 * @param gas    Gas initial (déjà déduit de l'intrinsic cost).
 * @return VMResult (ok, gasUsed, output).
 * @note Mutations de storage écrites directement dans `state`.
 */
VMResult vmExecute(State& state, const CallContext& ctx, uint64_t gas);

// ---------------------------------------------------------------
// Assembleur NVM
// ---------------------------------------------------------------
/**
 * @brief Assemble le source NVM en bytecode.
 * @param src Code source (voir syntaxe ci-dessous).
 * @return Bytecode binaire (vector<uint8_t>).
 *
 * Syntaxe :
 *   L_x:            Label (émet JUMPDEST)
 *   PUSH1..PUSH32 <hex|dec>   Valeur immédiate (taille minimale)
 *   PUSH_SEL "sig"            Selector = keccak256(sig)[0..3] << 224 (32 octets)
 *   JUMP L_x / JUMPI L_x      Auto-émis PUSH2 <addr> + JUMP/JUMPI
 *   Opcodes : STOP ADD SUB MUL DIV MOD LT GT EQ ISZERO AND OR XOR NOT
 *             SHL SHR CALLDATASIZE CALLDATALOAD CALLER ORIGIN ADDRESS
 *             CALLVALUE SLOAD SSTORE KECCAK RESULT REVERT JUMP JUMPI
 *             JUMPDEST POP DUP1..16 SWAP1..16
 *   Commentaires `//`
 *
 * Deux passes : 1) collecte labels → adresses, 2) émission bytecode.
 */
bytes assembleNvm(const std::string& src);

/**
 * @brief Calcule le selector ABI (premiers 4 octets de keccak256(signature)).
 * @param sig Signature canonique (ex: "transfer(address,uint256)").
 * @return 4 octets (utilisé par assembleur PUSH_SEL).
 */
fix32 abiSelector(const std::string& sig);

/**
 * @brief Extrait le i-ème mot 32 octets de la sortie VM.
 * @param output Sortie VM (concaténation de mots 32 octets).
 * @param i Index (0 = premier mot poussé par RESULT).
 * @return Mot 32 octets (zéro si hors limites).
 */
fix32 resultWord(const bytes& output, size_t i = 0);

// ---------------------------------------------------------------
// Template ERC-20
// ---------------------------------------------------------------
/**
 * @return Bytecode ERC-20 compilé (prêt à déployer).
 */
bytes erc20Bytecode();

/**
 * @brief Déploie un jeton ERC-20 pour `creator`.
 * @param state       État global (muté : code + storage initial).
 * @param creator     Adresse du créateur (doit avoir nonce).
 * @param creatorNonce Nonce actuel du créateur (AVANT incrément).
 * @param name        Nom du jeton (ASCII, max 32 chars, aligné gauche).
 * @param symbol      Symbole (ASCII, max 32 chars, aligné gauche).
 * @param decimals    Décimales (u8, stocké dans LSB du slot 3).
 * @param initialSupply Dotation initiale (mintée au créateur).
 * @return Adresse du contrat déployé.
 * @details Initialise storage :
 *   slot 0 = initialSupply (totalSupply)
 *   slot 1 = name (ASCII left-aligned 32 octets)
 *   slot 2 = symbol (ASCII left-aligned)
 *   slot 3 = decimals (LSB)
 *   balances[creator] = initialSupply (slot 4 mapping)
 */
fix20 erc20Deploy(State& state, const fix20& creator, const u256& creatorNonce,
                  const std::string& name, const std::string& symbol,
                  uint8_t decimals, const u256& initialSupply);

} // namespace nstrike