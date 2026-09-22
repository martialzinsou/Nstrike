# ERC-20 Template — Contrat Jeton Compilé en NVM

> **Module** : `src/vm.hpp/.cpp` — `erc20Bytecode()`, `erc20Deploy()`

---

## 📘 About

**Description** : Fournir un jeton ERC-20 complet prêt à déployer, écrit en NVM assembly (compilé par `assembleNvm`), sans Solidity ni compilateur externe. Layout storage avec 6 slots clés (totalSupply, name, symbol, decimals, balances, allowance). Dispatch par selector sur les 4 premiers octets du calldata. 9 fonctions implémentées : totalSupply, name, symbol, decimals, balanceOf, transfer, approve, transferFrom, allowance. Fonction `erc20Deploy` pour déploiement avec init storage et mintage creator.

**Tags** : #erc20 #token #smart-contract #nvm #assembly #bytecode #deploy #mintage #solidity-alternative  
> **Auteur** : Martial Zinsou

---

## 🎯 Objectif

Fournir un **jeton ERC-20 complet** prêt à déployer, écrit en **NVM assembly** (compilé par `assembleNvm`), sans Solidity ni compilateur externe.

---

## 📦 Layout Storage (Slots)

| Slot | Clé | Contenu | Type |
|------|-----|---------|------|
| 0 | `word(0)` | `totalSupply` | `u256` |
| 1 | `word(1)` | `name` (ASCII left-aligned, 32 octets) | `fix32` |
| 2 | `word(2)` | `symbol` (ASCII left-aligned) | `fix32` |
| 3 | `word(3)` | `decimals` (u8 dans LSB) | `u256` |
| 4 | `keccak(addr32 \| word(4))` | `balances[addr]` | `u256` |
| 5 (inner) | `keccak(owner32 \| word(5))` | `allowance_inner` | `fix32` |
| 5 (outer) | `keccak(spender32 \| inner)` | `allowance[owner][spender]` | `u256` |

> **Note** : `word(s) = u256::toBytes(u256(s))` (32 octets big-endian).  
> `addr32 = u256::toBytes(u256::fromBytes(addr))` (adresse right-aligned dans 32 octets).

---

## 🔀 Dispatch par Selector

Le premier mot du calldata (offset 0) = selector aligné à gauche (`keccak(sig)[0..3] << 224`).

```
CALLDATASIZE PUSH1 4 LT JUMPI L_revert
PUSH32 0 CALLDATALOAD
PUSH_SEL "totalSupply()"      EQ JUMPI L_total
PUSH_SEL "name()"             EQ JUMPI L_name
PUSH_SEL "symbol()"           EQ JUMPI L_symbol
PUSH_SEL "decimals()"         EQ JUMPI L_decimals
PUSH_SEL "balanceOf(address)" EQ JUMPI L_balance
PUSH_SEL "transfer(address,uint256)"       EQ JUMPI L_transfer
PUSH_SEL "approve(address,uint256)"        EQ JUMPI L_approve
PUSH_SEL "transferFrom(address,address,uint256)" EQ JUMPI L_from
PUSH_SEL "allowance(address,address)"      EQ JUMPI L_allow
REVERT
```

---

## 📋 Fonctions Implémentées (9)

| Fonction | Selector | Args (offset) | Retour | Description |
|----------|----------|---------------|--------|-------------|
| `totalSupply()` | 0x18160ddd | — | `supply` | Offre totale |
| `name()` | 0x06fdde03 | — | `name` (32 octets) | Nom du jeton |
| `symbol()` | 0x95d89b41 | — | `symbol` | Symbole |
| `decimals()` | 0x313ce567 | — | `decimals` (u8) | Décimales |
| `balanceOf(address)` | 0x70a08231 | `addr`@4 | `balance` | Solde d'une adresse |
| `transfer(addr,uint)` | 0xa9059cbb | `to`@4, `amt`@36 | `1`/`0` | Transfert |
| `approve(addr,uint)` | 0x095ea7b3 | `spender`@4, `amt`@36 | `1`/`0` | Autorisation |
| `transferFrom(f,t,amt)` | 0x23b872dd | `from`@4, `to`@36, `amt`@68 | `1`/`0` | Transfert délégué |
| `allowance(o,s)` | 0xdd62ed3e | `owner`@4, `spender`@36 | `allowance` | Autorisation restante |

---

## 🔧 Implémentation Clé (Extraits)

### balanceOf
```asm
L_balance:
    PUSH32 4 CALLDATALOAD        // addr
    PUSH1 4 KECCAK SLOAD         // key=keccak(addr||4), load
    PUSH1 1 RESULT STOP
```

### transfer
```asm
L_transfer:
    PUSH32 4 CALLDATALOAD        // to
    DUP1 ISZERO JUMPI L_revert   // reject to==0
    CALLER PUSH1 4 KECCAK DUP1 SLOAD  // keyC, balC
    PUSH32 36 CALLDATALOAD       // amount
    DUP2 LT ISZERO JUMPI L_revert // balC < amount → revert
    SWAP1 SUB SSTORE             // balC -= amount
    PUSH32 4 CALLDATALOAD PUSH1 4 KECCAK DUP1 SLOAD
    PUSH32 36 CALLDATALOAD ADD SSTORE // bal(to) += amount
    PUSH1 1 RESULT STOP
```

### approve
```asm
L_approve:
    CALLER PUSH1 5 KECCAK        // inner = keccak(caller||5)
    PUSH32 4 CALLDATALOAD SWAP1 KECCAK // key = keccak(spender||inner)
    PUSH32 36 CALLDATALOAD SSTORE        // allowance = amount
    PUSH1 1 RESULT STOP
```

### transferFrom
```asm
L_from:
    PUSH32 4 CALLDATALOAD PUSH1 5 KECCAK // innerF
    CALLER SWAP1 KECCAK DUP1 SLOAD       // keyA, allowed
    PUSH32 68 CALLDATALOAD               // amount (arg2)
    DUP2 LT ISZERO JUMPI L_revert        // allowed < amount → revert
    SWAP1 SUB SSTORE                     // allowed -= amount
    // debit from
    PUSH32 4 CALLDATALOAD PUSH1 4 KECCAK DUP1 SLOAD
    PUSH32 68 CALLDATALOAD
    DUP2 LT ISZERO JUMPI L_revert
    SWAP1 SUB SSTORE
    // credit to
    PUSH32 36 CALLDATALOAD PUSH1 4 KECCAK DUP1 SLOAD
    PUSH32 68 CALLDATALOAD ADD SSTORE
    PUSH1 1 RESULT STOP
```

---

## 🚀 Déploiement (`erc20Deploy`)

```cpp
fix20 erc20Deploy(State& state, const fix20& creator, const u256& creatorNonce,
                  const std::string& name, const std::string& symbol,
                  uint8_t decimals, const u256& initialSupply);
```

1. `addr = deriveContractAddress(creator, creatorNonce)`
2. `state.setCode(addr, erc20Bytecode())`
3. Storage init :
   - `slot 0 = initialSupply`
   - `slot 1 = name` (ASCII left-aligned 32 octets)
   - `slot 2 = symbol`
   - `slot 3 = decimals` (LSB)
   - `balances[creator] = initialSupply` via `keccak(creator||word(4))`
4. `creatorNonce++`

---

## 📂 Fichiers

| Fichier | Contenu |
|---------|---------|
| `vm.hpp` | API `erc20Bytecode()`, `erc20Deploy()` |
| `vm.cpp` | Source assembleur `ERC20_SRC` + implémentation déploiement |

---

## 🔗 Voir aussi

- [EIP-20](https://eips.ethereum.org/EIPS/eip-20) — Standard ERC-20
- [ABI Specification](https://docs.soliditylang.org/en/latest/abi-spec.html) — Selector encoding
- [Virtual-Machine-NVM](Virtual-Machine-NVM) — VM & assembleur