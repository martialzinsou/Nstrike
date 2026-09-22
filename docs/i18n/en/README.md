# Nstrike — Lightweight Blockchain (MVP)

> **Author** : Martial Zinsou  
> **Version** : 0.1.0 (MVP)  
> **Year** : 2026  
> **License** : MIT  
> **Site** : https://github.com/martialzinsou/Nstrike

---

## 📋 Table of Contents

1. [Project Overview](#project-overview)
2. [Installation and Usage](#installation-and-usage)
3. [CLI Commands](#cli-commands)
4. [Building the Project](#building-the-project)
5. [Testing](#testing)
6. [Project Structure](#project-structure)

---

## 🎯 Project Overview

**Nstrike** is a lightweight (MVP) blockchain implementation entirely **from scratch in C++17**, with **no external dependencies**. The project aims to provide a solid technical foundation for a private/consortium blockchain with essential Ethereum-like features:

- **Proof-of-Work Consensus** (PoW) with head-zeros
- **Block consensus** with difficulty adjustment
- **Accounts** (accounts, nonce, balance)
- **Transactions** (ECDSA signature, gas)
- **Mini-VM** (NVM) for contract execution
- **ERC-20 Template** (standard token)
- **JSON-RPC** (2.0 interface)
- **CLI** (command-line interface)

---

## 📦 Technical Architecture

```
+---------------------+     +-----------------+     +-----------------+
|     CLI (main.cpp)  | --> | RPC (rpc.cpp)     | --> | Blockchain (core) |
+---------------------+     +-----------------+     +-----------------+
         |                       |                       |
         |   JSON-RPC       |               |   State (state.cpp)
         |                   +--> chain.cpp (blocks, PoW)
         +--> tokens/tokens.cpp (ERC-20 template)
              +--> vm.cpp (NVM VM + assembler)
                    +--> crypto.cpp (crypto primitives)
                          +--> sha256.hpp/.cpp
                          +--> keccak.hpp/.cpp
```

### Key Technical Constraints

- **Language** : C++17 only
- **Dependencies** : Zero external dependencies (everything from scratch)
- **Security** : Internal crypto audits, no third-party dependencies
- **Performance** : Light PoW for mobile/embedded environments
- **Documentation** : Complete documentation (Doxygen, Wiki, README)

---

## 📦 Installation and Usage

```bash
# Clone the repository
git clone https://github.com/martialzinsou/Nstrike.git
cd Nstrike

# Build
make            # build/nstrike (CLI)
make test       # 32 checks (0 failure)

# CLI Usage
./build/nstrike account new       # Create a new account
./build/nstrike balance <addr>    # Check balance
./build/nstrike send <priv> <to> <amt>  # Perform a transfer
./build/nstrike mine <addr> [count]  # Mine blocks
./build/nstrike rpc               # JSON-RPC server

# Deploy ERC-20 token
./build/nstrike token create <priv> "Name" "SYM" 18 <supply>

# Check chain state
./build/nstrike chain

# JSON-RPC server
./build/nstrike rpc
# (then send JSON-RPC requests via stdin)
```

---

## 🔧 Building the Project

```bash
make            # Compile the project
make test       # Run 32 verification checks
make clean      # Clean build artifacts
```

### Compilation Requirements

- **Compiler** : Clang++ 17 with `-std=c++17 -O2`
- **OS** : Tested on macOS ARM64 / x86_64
- **Memory** : Minimum 256 MB RAM (for VM execution)
- **Storage** : In-memory only (MVP, no disk persistence)

---

## 📚 Documentation

- **Wiki** : 10 pages in GitHub Wiki (Markdown)
- **Doxygen** : Complete documentation on all `.hpp`/`.cpp` files
- **README** : This file
- **Tests** : 32 unit tests in `tests/test_main.cpp`

---

## 🔗 Useful Links

- [GitHub Repository](https://github.com/martialzinsou/Nstrike)
- [Test Report](https://github.com/martialzinsou/Nstrike/actions)
- [Ethereum Yellow Paper](https://ethereum.github.io/yellowpaper/paper.pdf)
- [FIPS 180-4 (SHA-256)](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf)
- [FIPS 202 (SHA-3/Keccak)](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.202.pdf)
- [RFC 6979 (Deterministic ECDSA)](https://datatracker.ietf.org/doc/html/rfc6979)

---

## 📄 License

This project is licensed under **MIT**. See the `LICENSE` file for details.

---

## 📞 Contact

- **GitHub** : https://github.com/martialzinsou/Nstrike
- **Project** : https://github.com/martialzinsou/Nstrike
- **Author** : Martial Zinsou

---

*Documentation generated on [Current Date]*
*Open-source project under MIT license - free to use, modify, and redistribute.*