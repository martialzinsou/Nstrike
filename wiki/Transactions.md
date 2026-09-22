# Transactions — Sérialisation, Gas, Exécution

> **Module** : `src/tx.hpp/.cpp`

---

## 📘 About

**Description** : Structure transaction canonique sur 160 octets fixe + données variables. Champs : nonce (u256), gasPrice (u256), gasLimit (u256), to (fix20), value (u256), data (bytes). Validation préalable : chainId, signature ECDSA, nonce correspondance, solde suffisant. Modèle gas : 21000 base + 100*data.size(), création contrat +32000. Nonce politique : tx.nonce == state.nonce(sender), protection replay via CHAIN_ID. Types : transfert simple, création contrat, appel contrat.

**Tags** : #transaction #serialization #gas #ecdsa #nonce #replay-protection #chainid #modell-gas  
> **Auteur** : Martial Zinsou

---

## 📦 Structure Transaction

### Format Sérialisé (Big-endian)

```
tx[0..31]  nonce (u256, 32 octets)
tx[32..63] gasPrice (u256, 32 octets)
tx[64..95] gasLimit (u256, 32 octets)
tx[96..127] to (fix20, 20 octets right-aligned dans 32)
tx[128..159] value (u256, 32 octets)
tx[160.....] data (bytes variable)
```

### Champs

| Champ | Type | Description |
|-------|------|-------------|
| `nonce` | `u256` | Nonce du signataire (incrémente par bloc) |
| `gasPrice` | `u256` | Prix par unité de gas (nwei) |
| `gasLimit` | `u256` | Gas maximum alloué |
| `to` | `fix20` | Destinataire (0 = création contrat) |
| `value` | `u256` | Montant transféré (nwei) |
| `data` | `bytes` | Sélecteur + arguments d'appel |

### Validation Préalable (`validTxBase`)

```cpp
bool validTxBase(const Transaction& tx, const State& st) {
    1. tx.chainId == CHAIN_ID
    2. tx.verified() — signature ECDSA valide
    3. tx.nonce == st.nonce(tx.sender)
    4. balance(tx.sender) >= gasLimit*gasPrice + value
}
```

---

## ⛽ Modèle de Gas

### Gas Intrinsèque (coût de base)

| Opération | Gas |
|-----------|-----|
| Transaction simple (transfer) | `21000` |
| Données additionnelles | `+ 100 * data.size()` |
| Création contrat (CREATE) | `+ 32000` |
| SLOAD (lecture storage) | `100` |
| SSTORE (écriture storage) | `20000` (initial) / `5000` (réduction) |

### Frais de Gas Réels

```cpp
uint64_t computeGasUsed(const Transaction& tx, uint64_t gasConsumed) {
    return gasConsumed; // gas utilisé pendant l'exécution
}

// Remboursement (EIP-3529 inspiré)
uint64_t computeRefund(uint64_t gasUsed, uint64_t gasLimit) {
    uint64_t unused = gasLimit - gasUsed;
    // Maximum 50% du gas peut être remboursé
    return min(unused, gasLimit / 2);
}
```

### Distribution des Frais

```
Total frais = gasUsed * gasPrice
↳ Miner reçoit : rewardBlock + fees
⳾ Remboursé au signataire : refund * gasPrice
```

---

## 🔀 Types de Transactions

### 1. Transfert Simple

```cpp
// Opcode sequence (NVM assembly)
PUSH32 4 CALLDATALOAD        // to address
DUP1 ISZERO JUMPI L_revert   // to != 0
CALLER PUSH1 4 KECCAK DUP1 SLOAD  // key = keccak(caller||4), load balance
PUSH32 36 CALLDATALOAD       // amount
DUP2 LT ISZERO JUMPI L_revert // balance < amount → revert
SWAP1 SUB SSTORE             // balance -= amount
PUSH32 4 CALLDATALOAD PUSH1 4 KECCAK DUP1 SLOAD
PUSH32 36 CALLDATALOAD ADD SSTORE // balance(to) += amount
PUSH1 1 RESULT STOP
```

### 2. Création de Contrat

```asm
// CREATE opcode
PUSH1 0 CALLDATALOAD         // salt (optionnel)
PUSH1 4 KECCAK               // keccak(sender||4)
PUSH32 32 CALLDATALOAD       // bytecode
PUSH1 0 SSTORE               // stockage code initial
// ... deployment logic
```

### 3. Appel de Contrat

```asm
// CALL opcode
PUSH1 0 CALLDATALOAD         // value transférée
PUSH1 0 CALLDATASIZE         // size de données
PUSH1 0 CALLDATALOAD         // selector + args
// ... vmExecute avec gas restant
```

---

## 📝 Nonce & Replay Protection

### Politique de Nonce

- `tx.nonce` doit égaler `state.nonce(sender)` exactement
- Après exécution : `++state.nonce(sender)`
- `CHAIN_ID` inclus dans la signature ECDSA (EIP-155 style)
- Protection replay : `sig valid uniquement pour chainId courant`

### Vérification (`verifySignature`)

```cpp
bool Transaction::verifySignature() const {
    // 1. Récupérer le message (signedData)
    bytes message = signs.data(); // tout le serialized tx sans r,s
    
    // 2. Récupérer la clé publique depuis la signature
    fix20 pubKey = ecrecover(message, v, r, s);
    
    // 3. Vérifier que l'adresse correspond
    fix20 expectedAddr = deriveAddress(pubKey);
    return expectedAddr == sender;
}
```

---

## 📂 Fichiers

| Fichier | Contenu |
|---------|---------|
| `tx.hpp` | `Transaction` struct, API serialization |
| `tx.cpp` | Sérialisation, validation, calcul gas |

---

## 🔗 Voir aussi

- [State-and-Accounts](State-and-Accounts) — Stockage des comptes
- [Virtual-Machine-NVM](Virtual-Machine-NVM) — Exécution des opérations
- [Chain-and-Consensus](Chain-and-Consensus) — Validation bloc + mempool