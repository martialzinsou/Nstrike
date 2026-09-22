# RPC Specification — JSON-RPC 2.0

> **Module** : `src/rpc.hpp/.cpp`

---

## 📘 About

**Description** : Serveur JSON-RPC 2.0 en stdio avec 9 endpoints supportés : getbalance, sendtx, getblock, getblockcount, getgasprice, mine, chainstatus, deploytoken, callcontract. Format requête/réponse standard JSON-RPC 2.0. Formats de données : address (20 octets hex), quantity/u256 (64 hex chars), boolean, string. Codes d'erreur : -32700 (parse), -32600 (invalid request), -32601 (method not found), -32602 (invalid params), -32603 (internal). Mode interaction : lecture stdin, écriture stdout.

**Tags** : #json-rpc #rpc #stdio #api #endpoints #json #method #getbalance #sendtx #getblock  
> **Auteur** : Martial Zinsou

---

## 🌐 Endpoints JSON-RPC Supportés

### Requête JSON-RPC 2.0 Standard

```json
{
  "jsonrpc": "2.0",
  "method": "<method_name>",
  "params": [...],
  "id": <request_id>
}
```

### Réponse JSON-RPC 2.0 Succès

```json
{
  "jsonrpc": "2.0",
  "result": <result>,
  "id": <request_id>
}
```

### Réponse JSON-RPC 2.0 Erreur

```json
{
  "jsonrpc": "2.0",
  "error": {
    "code": <error_code>,
    "message": "<error_message>"
  },
  "id": <request_id>
}
```

---

## 📋 Méthodes Supportées

### 1. `getbalance`

**Description** : Récupérer le solde d'une adresse.

**Paramètres**
```json
[{"address": "0xabc123..."}]
```

**Retour**
```json
"0x0000000000000000000000000000000000000000000000000000000000000000" (hex string, nwei)
```

**Exemple**
```bash
curl -X POST http://localhost:8080 \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"getbalance","params":[{"address":"0xabc123"}],"id":1}'
```

---

### 2. `sendtx`

**Description** : Envoyer une transaction signée.

**Paramètres**
```json
[{"tx": "0x..."}]  // Transaction hexadécimale sérialisée
```

**Retour**
```json
"0x<hash_32_bytes_hex>" (hash de la transaction)
```

**Exemple**
```bash
curl -X POST http://localhost:8080 \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"sendtx","params":[{"tx":"0x0123456789abcdef..."}],"id":1}'
```

---

### 3. `getblock`

**Description** : Récupérer les informations d'un bloc par hauteur.

**Paramètres**
```json
[{"height": 5}]
```

**Retour**
```json
{
  "hash": "0x...",
  "height": 5,
  "timestamp": 1234567890,
  "miner": "0x...",
  "txCount": 3,
  "transactions": ["0x...", "0x...", "0x..."]
}
```

---

### 4. `getblockcount`

**Description** : Récupérer la hauteur actuelle du bloc (dernier bloc).

**Paramètres** : `[]` (vide)

**Retour**
```json
"0x5" (hex string, hauteur actuelle)
```

---

### 5. `getgasprice`

**Description** : Récupérer le prix de gas suggéré.

**Paramètres** : `[]` (vide)

**Retour**
```json
"0x1" (hex string, 1 nwei = prix minimum)
```

**Remarque** : Prix fixe de 1 nwei par unité de gas (configuration par défaut).

---

### 6. `mine`

**Description** : Miner des blocs pour une adresse donnée.

**Paramètres**
```json
[{"miner": "0xabc123...", "count": 1}]
```

**Retour**
```json
{
  "status": "success",
  "blocksMined": 1,
  "newHeight": 1,
  "reward": "0x32000000000000000000"  // 50 NST en hex
}
```

---

### 7. `chainstatus`

**Description** : Récupérer l'état complet de la chaîne.

**Paramètres** : `[]` (vide)

**Retour**
```json
{
  "status": "synced",
  "currentHeight": 15,
  "difficultyBits": 16,
  "blockTime": 15,
  "chainId": 1
}
```

---

### 8. `deploytoken`

**Description** : Déployer un contrat jeton ERC-20.

**Paramètres**
```json
[{"creator": "0xabc123...", "name": "MyToken", "symbol": "MTK", "decimals": 18, "initialSupply": "0x1000000000000000000"}]
```

**Retour**
```json
"0xabc123..." (adresse du contrat déployé)
```

---

### 9. `callcontract`

**Description** : Appeler une fonction de contrat.

**Paramètres**
```json
[{"contract": "0xabc123...", "data": "0x..." , "gas": "0x5208", "from": "0xdef456..."}]
```

**Retour**
```json
"0x..." (résultat de l'appel VM, hex string)
```

---

## 🔌 Serveur RPC en Ligne de Commande (`rpcServeStdio`)

### Mode d'Interaction

Le serveur RPC lit les requêtes JSON depuis **stdin** et renvoie les réponses sur **stdout**.

```bash
# Démarrage
./build/nstrike rpc

# Envoi d'une requête (via un autre terminal ou pipe)
echo '{"jsonrpc":"2.0","method":"getbalance","params":[{"address":"0xabc123"}],"id":1}' | ./build/nstrike rpc

# Sortie attendue
{"jsonrpc":"2.0","result":"0x0000000000000000000000000000000000000000000000000000000000000000","id":1}
```

### Implémentation C++

```cpp
void rpcServeStdio(Chain& chain) {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        
        Json req;
        if (!Json::tryParse(line, req)) {
            Json resp = errResp(-32700, "parse error", req.value("id", Json::Null));
            fputs(resp.dump().c_str(), stdout);
            fputc('\n', stdout);
            fflush(stdout);
            continue;
        }
        
        Json resp = rpcHandle(req, chain);
        fputs(resp.dump().c_str(), stdout);
        fputc('\n', stdout);
        fflush(stdout);
    }
}
```

### Gestion des Erreurs

| Code | Message | Description |
|------|---------|-------------|
| `-32700` | Parse error | Requête JSON invalide |
| `-32600` | Invalid Request | Méthode inconnue ou version JSON-RPC non supportée |
| `-32601` | Method not found | La méthode n'existe pas |
| `-32602` | Invalid params | Paramètres invalides |
| `-32603` | Internal error | Erreur interne au traitement |

---

## 📦 Formats de Données

### Adresse (`address`)

- Format : 20 octets right-aligned dans 32 octets
- Hexadécimal : `0x` + 40 caractères hexadécimaux
- Exemple : `0xabc123def4567890123456789012345678901234`

### Nonce (`quantity` / `u256`)

- Format : hexadécimal non signé 256 bits
- Hexadécimal : `0x` + 64 caractères hexadécimaux
- Exemple : `0x0000000000000000000000000000000000000000000000000000000000000000`

### Booléen (`boolean`)

- `true` / `false` (JSON standard)

### Chaîne (`string`)

- JSON standard, UTF-8 supporté
- Exemple : `"0xabc123..."`

---

## 📂 Fichiers

| Fichier | Contenu |
|---------|---------|
| `rpc.hpp` | Définitions d'endpoints, structures de requête/réponse |
| `rpc.cpp` | Implémentation serveur JSON-RPC, gestion des méthodes |

---

## 🔗 Voir aussi

- [CLI-Reference](CLI-Reference) — Commandes en ligne de commande
- [Virtual-Machine-NVM](Virtual-Machine-NVM) — Appels de contrat via RPC
- [Chain-and-Consensus](Chain-and-Consensus) — État de la chaîne, minage