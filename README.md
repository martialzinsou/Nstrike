# Nstrike — Chaîne de blocs légère (MVP)

> **Auteur** : Martial Zinsou  
> **Version** : 0.1.0 (MVP)  
> **Licence** : MIT  
> **Langage** : C++17 (sans dépendance externe)

---

## 📖 Description

**Nstrike** est une implémentation *from scratch* d'une chaîne de blocs de type Ethereum, écrite entièrement en C++17 moderne sans aucune bibliothèque externe (ni OpenSSL, ni Boost, ni secp256k1). Le projet vise à démontrer le fonctionnement interne d'une blockchain complète : cryptographie, machines virtuelles, consensus Proof-of-Work, comptes, jetons ERC-20, RPC JSON et CLI.

### Fonctionnalités principales

| Composant | Description |
|-----------|-------------|
| **Cryptographie** | SHA-256, Keccak-256 (permutation Keccak-f[1600]), secp256k1 (ECDSA, RFC 6979, ecrecover), adresses 20 octets style Ethereum |
| **Arithmétique 256 bits** | `u256` avec addition/soustraction modulaire, multiplication de Montgomery, inversion modulaire (Fermat), exponentiation modulaire, division exacte |
| **Comptes & État** | Nonce, solde, code contrat, storage (map clé→valeur), racine d'état canonique (Keccak cumulatif) |
| **Transactions** | Sérialisation canonique (magic + big-endian), signature ECDSA + recovery ID, vérification expéditeur (`ecrecover`), gas |
| **Mini-VM (NVM)** | Interpréteur pile 32 octets, opcodes style EVM (arithmétique, sauts, storage, calldata, Keccak, résultat), assembleur avec labels & `PUSH_SEL` |
| **Jeton ERC-20** | Template compilé en NVM : `totalSupply`, `balanceOf`, `transfer`, `approve`, `allowance`, `transferFrom`, `name`, `symbol`, `decimals` |
| **Chaîne & Consensus** | Blocs (en-tête + txs), PoW léger (zéros de tête Keccak double), difficulté ajustable (fenêtre 24 blocs), récompense 50 NST + halving 210 000 blocs, mempool |
| **RPC JSON 2.0** | `getbalance`, `sendtx`, `getblock`, `getblockcount`, `getgasprice`, `mine`, `chainstatus`, `deploytoken`, `callcontract` |
| **CLI** | `account new/list`, `address`, `balance`, `send`, `token create/transfer/balance`, `mine`, `chain`, `rpc` |

---

## 🏗️ Architecture

```
Nstrike/
├── src/
│   ├── config.hpp        # Constantes du protocole
│   ├── common.hpp/.cpp   # Types de base, hex, u256 (Montgomery), aléa
│   ├── sha256.hpp/.cpp   # SHA-256 from scratch
│   ├── keccak.hpp/.cpp   # Keccak-256 / SHA3-256 + Keccak-f[1600]
│   ├── crypto.hpp/.cpp   # secp256k1, ECDSA, clés, adresses
│   ├── json.hpp/.cpp     # Parseur/sérialiseur JSON minimal (RFC 8259)
│   ├── state.hpp/.cpp    # Comptes, storage, racine d'état
│   ├── tx.hpp/.cpp       # Transaction, signature, hash, sérialisation
│   ├── vm.hpp/.cpp       # Mini-VM NVM, assembleur, template ERC-20
│   ├── chain.hpp/.cpp    # Blocs, PoW, difficulté, récompense, mempool
│   ├── rpc.hpp/.cpp      # Serveur JSON-RPC 2.0 (stdio)
│   └── main.cpp          # CLI (wallet, envoi, minage, jetons, RPC)
├── tests/
│   └── test_main.cpp     # Tests unitaires (vecteurs + intégration)
├── assets/
│   └── logo.svg          # Logo Nstrike (style Microsoft)
├── Makefile              # Build (make, make test, make run, make clean)
└── README.md             # Ce fichier
```

---

## ⚙️ Prérequis

- **macOS / Linux** (testé sur macOS ARM64 & x86_64, Linux x86_64)
- **Clang ≥ 10** ou **GCC ≥ 9** (support C++17 complet)
- `make`

> ⚠️ Aucune dépendance externe (OpenSSL, Boost, etc.) n'est requise.

---

## 🚀 Installation & Build

```bash
# Cloner le dépôt
git clone https://github.com/<votre-utilisateur>/Nstrike.git
cd Nstrike

# Compiler (binaire + tests)
make            # → build/nstrike (CLI)
make test       # → lance la suite de tests (32 vérifications)

# Nettoyer
make clean
```

### Résultat attendu des tests

```
[ OK ] sha256_vectors
[ OK ] keccak256_vectors
[ OK ] u256_basics
[ OK ] u256_modular
[ OK ] ecc_basics
[ OK ] ecdsa_roundtrip
[ OK ] ecdsa_many
[ OK ] hex_roundtrip

8 test(s), 32 vérification(s), 0 échec(s)
```

---

## 💻 Utilisation (CLI)

### Portefeuille

```bash
# Créer un nouveau compte (clé privée + adresse)
./build/nstrike account new
# → Nouveau compte : 0xabc123...

# Lister les comptes du wallet (~/.nstrike/wallet.json)
./build/nstrike account list
```

### Minage (Proof-of-Work)

```bash
# Miner 1 bloc pour l'adresse donnée (récompense 50 NST + frais)
./build/nstrike mine 0x3ab19ecdaea8a14f4bb2a6f07fa4801e40be7c1f 1

# Miner 5 blocs
./build/nstrike mine 0x3ab19... 5
```

> ⚠️ **Limitation MVP** : la chaîne n'est pas persistée sur disque. Chaque invocation CLI crée une nouvelle chaîne en mémoire. Pour un minage réel, utilisez le serveur RPC (voir ci-dessous) ou intégrez la persistance (RocksDB, LevelDB, etc.).

### Solde

```bash
./build/nstrike balance 0x3ab19ecdaea8a14f4bb2a6f07fa4801e40be7c1f
```

### Transfert NST

```bash
# <privHex> <dest> <montant en nwei> [gasPrice en nwei]
./build/nstrike send 0x2e066... 0xabc123... 0x0de0b6b3a7640000 0x1
```

### Jeton ERC-20

```bash
# Déployer un jeton (créateur doit avoir un compte dans le wallet)
./build/nstrike token create 0x3ab19... "MonToken" "MTK" 18 0x56bc75e2d63100000

# Transférer des jetons
./build/nstrike token transfer 0x3ab19... 0xTokenAddr 0xDestAddr 0x1000

# Consulter le solde d'un jeton
./build/nstrike token balance 0xTokenAddr 0xUserAddr
```

### État de la chaîne

```bash
./build/nstrike chain
```

### Serveur JSON-RPC (stdio)

```bash
# Lance le serveur RPC sur l'entrée/sortie standard (Ctrl-D pour quitter)
./build/nstrike rpc
```

Exemple de requête RPC :

```json
{"jsonrpc":"2.0","method":"getbalance","params":{"address":"0x3ab19..."},"id":1}
```

Réponse :

```json
{"jsonrpc":"2.0","result":"0x2b5e3af16b1880000","id":1}
```

---

## 📡 API JSON-RPC 2.0

| Méthode | Paramètres | Description |
|---------|------------|-------------|
| `getbalance` | `{"address": "0x..."}` | Solde NST (nwei) |
| `sendtx` | `{"tx": "0x..."}` | Envoie une transaction signée (mempool) |
| `getblock` | `{"height": 0}` | Infos bloc (hash, hauteur, timestamp, miner, nb tx) |
| `getblockcount` | `{}` | Hauteur actuelle |
| `getgasprice` | `{}` | Prix de gas suggéré (1 nwei) |
| `mine` | `{"miner":"0x...","count":1}` | Mine `count` blocs |
| `chainstatus` | `{}` | Hauteur, difficulté, mempool, stateRoot |
| `deploytoken` | `{"creator":"0x...","nonce":0,"name":"...","symbol":"...","decimals":18,"supply":"0x..."}` | Déploie un ERC-20 |
| `callcontract` | `{"contract":"0x...","data":"0x...","gas":50000,"from":"0x..."}` | Appel lecture seule (VM) |

---

## 🧪 Tests

```bash
make test
```

La suite vérifie :

- Vecteurs officiels SHA-256 & Keccak-256 (FIPS 202 / NIST)
- Permutation Keccak-f[1600] (vecteur XKCP `perm(0) = f1258f7940e1dde7...`)
- Arithmétique `u256` : add/sub/mul/div/mod/inv/pow modulaire
- secp256k1 : multiplication scalaire, addition, doublage, ECDSA (signature, vérification, récupération)
- Round-trip hex/JSON
- ERC-20 : déploiement, `transfer`, `balanceOf`, `approve`, `allowance`, `transferFrom`

---

## 📚 Wiki GitHub (Structure recommandée)

> Après avoir poussé sur GitHub, créez les pages Wiki suivantes via l'onglet **Wiki** du dépôt :

### 1. **Home** — Vue d'ensemble
- Résumé du projet, objectifs, public cible (éducation, recherche, prototypage)
- Diagramme d'architecture (Mermaid)

### 2. **Cryptography** — Détails crypto
- SHA-256 : constantes, compression, padding
- Keccak-256 : taux 136, domaine 0x01, permutation f[1600] (θ, ρ, π, χ, ι), vecteurs de test
- secp256k1 : paramètres (p, n, G), coordonnées affines & Jacobien, formules d'addition/doublage
- ECDSA : RFC 6979 (k déterministe HMAC-SHA256), récupération `recid` (0..3)
- Adresses : `keccak256(pub[1..64])[12..32]`

### 3. **U256 Arithmetic** — Arithmétique 256 bits
- Représentation little-endian 4×u64
- Addition/soustraction modulaire (sans retenue haute)
- Multiplication de Montgomery : REDC (CIOS), `R = 2^256`, `m' = -p⁻¹ mod 2^64`
- Inversion modulaire : `a^(p-2) mod p` (exponentiation binaire gauche→droite)
- Division exacte (algorithme shift-and-subtract 256 itérations)

### 4. **Virtual Machine (NVM)** — Mini-VM
- Jeu d'opcodes (table complète avec coûts de gas)
- Pile de mots 32 octets (max 1023)
- Calldata : `CALLDATALOAD(offset)`, `CALLDATASIZE`
- Storage : `SLOAD(key)`, `SSTORE(key,value)` — clé = mot 32 octets
- Keccak : `KECCAK` pop a,b → push keccak(a||b) — utilisé pour clés de mapping
- Sauts : `JUMP(dest)`, `JUMPI(dest,cond)` — `dest` doit être `JUMPDEST`
- Résultat : `RESULT(n)` → sortie `n` mots
- Assembler : labels `L_x:`, `PUSH1..PUSH32`, `PUSH_SEL "sig"`, `JUMP L_x` / `JUMPI L_x` (auto PUSH2)

### 5. **ERC-20 Template** — Contrat jeton
- Layout storage :
  - slot 0 : `totalSupply`
  - slot 1 : `name` (ASCII left-aligned 32 octets)
  - slot 2 : `symbol`
  - slot 3 : `decimals` (u8 dans LSB)
  - slot 4 : `balances[addr]` → clé = `keccak(addr||word(4))`
  - slot 5 : `allowance[owner][spender]` → clé = `keccak(spender||keccak(owner||5))`
- Dispatch par selector (premiers 4 octets de `keccak(sig)` alignés à gauche)
- Fonctions : `totalSupply`, `name`, `symbol`, `decimals`, `balanceOf`, `transfer`, `approve`, `transferFrom`, `allowance`

### 6. **Chain & Consensus** — Chaîne & consensus
- En-tête : version, hauteur, timestamp, difficulté (bits), prevHash, stateRoot, txRoot, miner, nonce
- Hachage bloc : Keccak(sérialisation big-endian)
- PoW : `Keccak(Keccak(header))` doit avoir `difficulty` zéros de tête (MSB first)
- Difficulté : fenêtre 24 blocs, cible 15 s/bloc
  - Si temps réel > 3× attendu → difficulté –1
  - Si temps réel < 1/3 attendu → difficulté +1
  - Bornes [16, 32] bits
- Récompense : `BASE_REWARD = 50 NST = 5×10¹⁹ nwei`, halving tous les 210 000 blocs (`shrBits(1)` itératif)
- Mempool : validation complète (`runTx` sur copie d'état) avant insertion
- Application bloc : exécution séquentielle, remboursement gas non consommé, frais au mineur, stateRoot final

### 7. **CLI Reference** — Référence CLI
- Tableau complet commandes/sous-commandes/arguments
- Exemples copiables
- Format wallet JSON (`~/.nstrike/wallet.json`)

### 8. **RPC Specification** — Spécification RPC
- Format JSON-RPC 2.0 (request/response, erreurs standard)
- Liste exhaustive méthodes, paramètres, codes d'erreur personnalisés (-32000 tx invalide)

### 9. **Limitations & Roadmap** — Limitations & feuille de route
- **Pas de persistance** : état perdu à chaque processus (MVP)
- **Pas de P2P** : pas de réseau, pas de synchronisation
- **Gas simplifié** : coûts fixes, pas de schedule EIP-1559
- **Pas de receipts / logs** : pas d'indexation événements
- **Feuille de route** : persistance (RocksDB), P2P (libp2p), sync, receipts, métriques, tests fuzzing

### 10. **Contributing** — Contribuer
- Style : C++17, `-Wall -Wextra`, pas de dépendances
- Tests : ajouter cas dans `tests/test_main.cpp`
- PR : CI (GitHub Actions) `make test` obligatoire

---

## 🤝 Contribution

Les contributions sont les bienvenues ! Merci de :

1. Forker le dépôt
2. Créer une branche `feature/ma-fonctionnalite`
3. Ajouter des tests dans `tests/test_main.cpp`
3. `make test` doit passer
4. Ouvrir une Pull Request

---

## 📄 Licence

Ce projet est sous licence **MIT** — voir le fichier [LICENSE](LICENSE) pour les détails.

---

## 🙏 Remerciements

- **XKCP** (Keccak Code Package) pour les vecteurs de test officiels
- **NIST FIPS 202** (SHA-3 / Keccak)
- **Standards Efficient Cryptography Group (SECG)** — secp256k1
- **Ethereum Yellow Paper** — modèle de référence comptes/gas/VM
- **RFC 6979** — ECDSA déterministe

---

> **Note** : Ce MVP est conçu à des fins éducatives et de prototypage. Il n'est **pas** destiné à la production (absence de persistance, P2P, audit de sécurité, etc.).