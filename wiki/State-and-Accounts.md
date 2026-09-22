# State and Accounts — Comptes, Stockage, Racines d'État

> **Module** : `src/state.hpp/.cpp`

---

## 📘 About

**Description** : Modèle de compte complet avec adresse (fix20), nonce (u256), balance (u256), codeHash (fix32), storageRoot (fix32). Slots storage : slot 0 = totalSupply, slot 1 = name, slot 2 = symbol, slot 3 = decimals, slot 4 = balances[addr] via keccak, slot 5 = allowance via double-keccak. Racines d'état : stateRoot(), storageRoot(), codeHash() implémentées avec keccak accumulative hashing. Validation nonce, protection replay CHAIN_ID.

**Tags** : #state #accounts #storage #root #hash #account-model #u256 #fix32 #keccak  
> **Auteur** : Martial Zinsou

---

## 👤 Modèle de Compte

### Structure Account

| Champ | Type | Description |
|-------|------|-------------|
| `address` | `fix20` | Adresse (20 octets, right-aligned dans 32 octets) |
| `nonce` | `u256` | Nonce du compte (incrémente à chaque tx) |
| `balance` | `u256` | Solde en nwei (1 NST = 10⁹ nwei) |
| `codeHash` | `fix32` | Hash du code bytecode (keccak256) |
| `storageRoot` | `fix32` | Racine d'arbre de stockage (patricia) |

### Déduction d'Adresse

```cpp
fix20 deriveAddress(const fix20& privKey) {
    // Keccak(privKey || 0x00) → last 20 bytes
    bytes k = Keccak256::hash(bytes(privKey.data(), privKey.size()) + bytes("\x00"));
    return fix20(k.end() - 20, k.end());
}
```

### Nonce & Replay Protection

- `state.nonce(addr)` → u256
- `tx.nonce` doit égaler `state.nonce(sender)` ✅
- `++state.nonce(sender)` après exécution réussie
- `CHAIN_ID` inclus dans la signature (protection replay cross-chain)

---

## 💾 Stockage Contract (Storage Model)

### Slots de Storage (32 octets = word)

| Slot | Clé (keccak) | Contenu | Type |
|------|--------------|---------|------|
| 0 | — | `totalSupply` (template ERC-20) | `u256` |
| 1 | — | `name` (template ERC-20, ASCII left-aligned) | `fix32` |
| 2 | — | `symbol` (template ERC-20) | `fix32` |
| 3 | — | `decimals` (template ERC-20, u8 LSB) | `u256` |
| 4 | `keccak(addr32 \| word(4))` | `balances[addr]` | `u256` |
| 5 | `keccak(spender32 \| keccak(owner32 \| word(5)))` | `allowance[owner][spender]` | `u256` |

### Accès Stockage

```cpp
u256 State::getBalance(const fix20& addr) const {
    fix32 key = Keccak256::hash(
        bytes(u256::toBytes(u256(4))).append(
        bytes(addr.data(), addr.size())
    ));
    return SLOAD(key);
}

void State::setBalance(const fix20& addr, u256 bal) {
    fix32 key = Keccak256::hash(
        bytes(u256::toBytes(u256(4))).append(
        bytes(addr.data(), addr.size())
    ));
    SSTORE(key, bal);
}

u256 State::getAllowance(const fix20& owner, const fix20& spender) const {
    fix32 innerKey = Keccak256::hash(
        bytes(spender.data(), spender.size()).append(
        Keccak256::hash(bytes(owner.data(), owner.size()))
    );
    fix32 key = Keccak256::hash(bytes(u256::toBytes(u256(5))).append(bytes(innerKey)));
    return SLOAD(key);
}

void State::setAllowance(const fix20& owner, const fix20& spender, u256 amt) {
    fix32 innerKey = Keccak256::hash(
        bytes(spender.data(), spender.size()).append(
        Keccak256::hash(bytes(owner.data(), owner.size()))
    );
    fix32 key = Keccak256::hash(bytes(u256::toBytes(u256(5))).append(bytes(innerKey)));
    SSTORE(key, amt);
}
```

### Stockage Universel (non-ERC-20)

Pour les comptes de base (code, storage patricia) :

| Slot | Clé | Contenu |
|------|-----|---------|
| `keccak(addr32 \| word(0))` | nonce |
| `keccak(addr32 \| word(1))` | balance |
| `keccak(addr32 \| word(2))` | codeHash |
| `keccak(addr32 \| word(3))` | storageRoot |

---

## 🌳 Racines d'État (State Roots)

### `stateRoot()` Global

```cpp
fix32 State::stateRoot() const {
    fix32 h = fix32(HASH_ZERO);
    for (auto& kv : accounts) {
        fix32 addrW = u256::toBytes(u256::fromBytes(k.first.data(), k.first.size()));
        fix32 nonceW = u256::toBytes(k.second.nonce);
        fix32 balW   = u256::toBytes(k.second.balance);
        fix32 codeW  = k.second.code.empty() ? HASH_ZERO : keccak256(k.second.code);
        fix32 storW  = storageRoot(k.first); // racine de storage du compte
        fix32 leaf = keccak(addrW.append(nonceW).append(balW).append(codeW).append(storW));
        h = keccak(h.append(leaf));
    }
    return h;
}
```

### `storageRoot(const fix20& account)`

```cpp
fix32 State::storageRoot(const fix20& a) const {
    fix32 h = fix32(HASH_ZERO);
    for (auto& sv : storage[a]) {
        fix32 keyW = u256::toBytes(u256::fromBytes(sv.first.data(), sv.first.size()));
        fix32 valW = u256::toBytes(sv.second);
        fix32 leaf = keccac(keyW.append(valW));
        h = keccak(h.append(leaf));
    }
    return h;
}
```

### `codeHash(const fix20& account)`

```cpp
fix32 State::codeHash(const fix20& a) const {
    auto code = getCode(a);
    return code.empty() ? HASH_ZERO : keccak256(code);
}
```

---

## 📂 Fichiers

| Fichier | Contenu |
|---------|---------|
| `state.hpp` | `Account`, `State` API, stockage, racines |
| `state.cpp` | Implémentation SLOAD/SSTORE, racines, adresses |

---

## 🔗 Voir aussi

- [Transactions](Transactions) — Utilisation du storage dans txs
- [Virtual-Machine-NVM](Virtual-Machine-NVM) — SLOAD/SSTORE en VM
- [Chain-and-Consensus](Chain-and-Consensus) — Blocs, état global