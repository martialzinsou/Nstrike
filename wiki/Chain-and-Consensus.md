<meta name="google-site-verification" content="NXMEj6ESFJFiBGZP43G1v36Kka9lC_4Wfum2OMRZTBU" />
# Chain & Consensus — Blocs, PoW, Difficulté, Récompense, Mempool

> **Module** : `src/chain.hpp/.cpp`

---

## 📘 About

**Description** : Structure de bloc avec header (version, height, timestamp, difficultyBits, prevHash, stateRoot, txRoot, miner, nonce) et body (vector<Transaction>). Algorithme PoW : Keccak256(double header serialization), validation par difficultéBits zéros de tête. Ajustement de difficulté tous les 24 blocs (fenêtre 24 * 15s). Récompense de base 50 NST avec halving tous les 210000 blocs. Mempool avec validation nonce, signature, solde, gas intrinsèque. Exécution séquentielle sur copie d'état avant commit bloc.

**Tags** : #blockchain #pow #difficulty #reward #halving #mempool #consensus #block #mine  
> **Auteur** : Martial Zinsou

---

## 🧱 Structure de Bloc

### BlockHeader
| Champ | Type | Description |
|-------|------|-------------|
| `version` | `uint32_t` | Version du protocole (1) |
| `height` | `uint32_t` | Hauteur du bloc (0 = genesis) |
| `timestamp` | `uint64_t` | Unix timestamp (secondes) |
| `difficultyBits` | `u256` | Bits de zéros de tête requis (MSB first) |
| `prevHash` | `fix32` | Hash du bloc précédent |
| `stateRoot` | `fix32` | Racine d'état après exécution des txs |
| `txRoot` | `fix32` | Racine des transactions (Keccak cumulatif) |
| `miner` | `fix20` | Adresse du mineur (bénéficiaire récompense) |
| `nonce` | `u256` | Preuve de travail (cherché par minage) |

### Block
```cpp
struct Block {
    BlockHeader header;
    std::vector<Transaction> txs;
    bytes serialize() const;
    static Block deserialize(const bytes& b);
};
```

### Sérialisation (Big-endian)
```
version(4) | height(4) | timestamp(8) | difficulty(32)
| prevHash(32) | stateRoot(32) | txRoot(32)
| miner(20) | nonce(32) | ntx(4) | [tx_len(4) + tx_data]*
```

---

## ⛏️ Proof-of-Work (PoW Léger)

### Algorithme
1. `h1 = Keccak256(header_serialisé)`
2. `h2 = Keccak256(h1)` (double hachage style Bitcoin)
3. **Valide si** `h2` a au moins `difficultyBits` zéros de tête (MSB first)

### Vérification (`validPoW`)
```cpp
bool validPoW() const {
    fix32 h = Keccak256(Keccak256(header_serialized));
    for i in 0..difficultyBits-1:
        if bit(h, i) != 0: return false;
    return true;
}
```

### Minage (`mineBlocks`)
- Pré-calcule la partie fixe de l'en-tête (sans nonce)
- Boucle `nonce = 0,1,2...` jusqu'à `validPoW()`
- Taux ~600k H/s sur CPU (Keccak optimisé)
- Difficulté par défaut 16 bits → ~1-10 sec sur CPU moderne

---

## 📈 Ajustement de Difficulté

### Paramètres
| Paramètre | Valeur | Description |
|-----------|--------|-------------|
| `TARGET_BLOCK_SECONDS` | 15 | Temps cible par bloc |
| `DIFFICULTY_ADJUST_WINDOW` | 24 | Fenêtre d'ajustement (blocs) |
| `DEFAULT_DIFFICULTY_BITS` | 16 | Difficulté initiale |

### Algorithme (`nextDifficulty`)
```cpp
uint32_t nextDifficulty(blocks, now):
    if blocks.size() <= WINDOW: return current
    actual = now - timestamp[blocks.size() - W - 1]
    expected = W * 15
    if actual > expected * 3: return max(16, cur - 1)   // trop lent → plus facile
    if actual < expected / 3: return min(32, cur + 1)   // trop rapide → plus dur
    return cur
```

- **Bornes** : [16, 32] bits
- **Fréquence** : Tous les 24 blocs (~6 min à 15s/bloc)

---

## 💰 Récompense & Halving

### Récompense de base
```
BASE_REWARD = "50000000000000000000"  // 50 NST = 5×10¹⁹ nwei
HALVING_INTERVAL = 210000  // blocs (~4 ans à 15s/bloc)
```

### Calcul (`rewardAt`)
```cpp
u256 rewardAt(height):
    r = BASE_REWARD
    halvings = height / 210000
    repeat halvings times: r >>= 1  // division exacte par 2
    return r
```

- Bloc 0–209999 : 50 NST
- Bloc 210000–419999 : 25 NST
- Bloc 420000–629999 : 12.5 NST
- etc.

### Frais de gas
- `fees = Σ (gasUsed_i * gasPrice_i)` pour toutes txs du bloc
- Total mineur = `reward + fees`
- Crédité via `state.addBalance(miner, total)`

---

## 📥 Mempool & Exécution Transactions

### Validation (`addTx` / `runTx`)
1. **Chain ID** : `tx.chainId == CHAIN_ID`
2. **Signature** : `tx.verified()` + `sender()` valide
3. **Nonce** : `tx.nonce == state.nonce(sender)`
4. **Solde** : `balance(sender) >= gasLimit*gasPrice + value`
5. **Gas intrinsèque** : `21000 + 100*data.size + (isCreate?32000:0)`
6. **Exécution** sur copie d'état (`State probe = state`)
   - Transfert simple : déduction solde + crédit destinataire
   - Création contrat : `deriveContractAddress`, `setCode`, `setStorage`
   - Appel contrat : `vmExecute` avec gas restant
7. **Remboursement** : `unused = gasLimit - consumed` → `refund = unused * gasPrice`
8. **Échec** → rollback (copie jetée), tx rejetée

### Application Bloc (`mineBlocks`)
1. Prend jusqu'à `MAX_TX_PER_BLOCK` (256) txs valides du mempool
2. Exécute séquentiellement sur `State target = state`
3. Accumule frais, met à jour `target`
4. Calcule `stateRoot = target.stateRoot()`
5. `txRoot = cumulativeRoot(txHashes)`
6. Mine nonce (PoW)
7. Commit : `state = target`, retire txs minées du mempool

---

## 🔗 Validation Bloc (`validateBlock`)
```cpp
bool validateBlock(b):
    if b.height != blocks.size(): return false
    if b.prevHash != (blocks.empty()? 0 : blocks.back().hash()): return false
    if !b.validPoW(): return false
    return true
```

---

## 📂 Fichiers

| Fichier | Contenu |
|---------|---------|
| `chain.hpp` | `BlockHeader`, `Block`, `Chain` API |
| `chain.cpp` | PoW, difficulté, récompense, mempool, exécution txs |

---

## 🔗 Voir aussi

- [State-and-Accounts](State-and-Accounts) — Racine d'état, comptes
- [Transactions](Transactions) — Sérialisation, gas, exécution
- [Virtual-Machine-NVM](Virtual-Machine-NVM) — Appels contrat via VM