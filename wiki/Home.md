# Nstrike — Wiki Principal

> **Auteur** : Martial Zinsou  
> **Version** : 0.1.0 (MVP)  
> **Dernière mise à jour** : 2026

---

## 🎯 Vue d'ensemble

**Nstrike** est une implémentation *from scratch* d'une **Blockchain** de type Ethereum, écrite entièrement en **C++17 moderne** sans aucune dépendance externe (ni OpenSSL, ni Boost, ni secp256k1, ni librairies big-int). Le projet vise à démontrer le fonctionnement interne complet d'une **Blockchain** :

- **Cryptographie** : SHA-256, Keccak-256, secp256k1/ECDSA/RFC6979
- **Arithmétique 256 bits** : `u256` avec Montgomery, inversion modulaire, exponentiation
- **État & Comptes** : Nonce, solde, code, storage, racine d'état canonique
*   **Transactions** : Sérialisation canonique, signature ECDSA, gas, récupération expéditeur
- **Mini-VM (NVM)** : Pile 32 octets, opcodes EVM-like, assembleur, template ERC-20
- **Consensus** : Proof-of-Work léger (Keccak double), difficulté ajustable, récompense/halving
- **RPC & CLI** : JSON-RPC 2.0 (stdio), CLI complet (wallet, mine, send, token, chain)

> ⚠️ **MVP** : Pas de persistance disque, pas de P2P, pas de receipts/logs. Conçu pour l'éducation, la recherche et le prototypage rapide.

---

## 📚 Navigation du Wiki

| Page | Description |
|------|-------------|
| [Cryptography](Cryptography) | SHA-256, Keccak-256, secp256k1, ECDSA, RFC 6979 |
| [U256-Arithmetic](U256-Arithmetic) | Entier 256 bits, Montgomery, inversion, exponentiation |
| [Virtual-Machine-NVM](Virtual-Machine-NVM) | Mini-VM pile, opcodes, assembleur, template ERC-20 |
| [ERC20-Template](ERC20-Template) | Contrat jeton compilé en NVM (9 fonctions) |
| [Chain-and-Consensus](Chain-and-Consensus) | Blocs, PoW, difficulté, récompense, halving, mempool |
| [State-and-Accounts](State-and-Accounts) | Comptes, storage, racine d'état, adresse contrat |
| [Transactions](Transactions) | Sérialisation, signature, gas, ecrecover |
| [CLI-Reference](CLI-Reference) | Commandes complètes (account, send, token, mine, chain, rpc) |
| [RPC-Specification](RPC-Specification) | JSON-RPC 2.0 : 9 méthodes, codes erreur |
| [Limitations-and-Roadmap](Limitations-and-Roadmap) | Limitations MVP & feuille de route |

---

## 🚀 Démarrage Rapide

```bash
# Build
make            # → build/nstrike
make test       # 32 vérifications (0 échec)

# CLI
./build/nstrike account new
./build/nstrike mine <addr> 1
./build/nstrike balance <addr>
./build/nstrike token create <priv> "Name" "SYM" 18 <supply>
./build/nstrike rpc          # JSON-RPC sur stdio
```

---

## 📦 Architecture du Code

```
src/
├── config.hpp          # Constantes protocole
├── common.hpp/.cpp     # Types, hex, u256 (Montgomery)
├── sha256.hpp/.cpp     # SHA-256 (FIPS 180-4)
├── keccak.hpp/.cpp     # Keccak-256 / Keccak-f[1600]
├── crypto.hpp/.cpp     # secp256k1, ECDSA, RFC6979
├── json.hpp/.cpp       # JSON minimal (RFC 8259)
├── state.hpp/.cpp      # Comptes, storage, racine d'état
├── tx.hpp/.cpp         # Transaction, signature, ecrecover
├── vm.hpp/.cpp         # NVM, assembleur, ERC-20 template
├── chain.hpp/.cpp      # Blocs, PoW, difficulté, récompense
├── rpc.hpp/.cpp        # JSON-RPC 2.0 (stdio)
└── main.cpp            # CLI (wallet, mine, send, token...)
tests/
└── test_main.cpp       # 32 vérifications (vecteurs FIPS/XKCP)
```

---

## 🔗 Liens Utiles

- [Dépôt GitHub](https://github.com/martialzinsou/Nstrike)
- [Rapport de Tests](https://github.com/martialzinsou/Nstrike/actions)
- [Spécification Ethereum (Yellow Paper)](https://ethereum.github.io/yellowpaper/paper.pdf)
- [FIPS 180-4 (SHA-256)](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf)
- [FIPS 202 (SHA-3/Keccak)](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.202.pdf)
- [RFC 6979 (ECDSA déterministe)](https://datatracker.ietf.org/doc/html/rfc6979)

---

## 📄 Licence

MIT — voir [LICENSE](../LICENSE).

---

> **Auteur** : Martial Zinsou  
> **Contact** : voir profil GitHub