# Cryptography — SHA-256, Keccak-256, secp256k1, ECDSA

> **Module** : `src/sha256.hpp/.cpp`, `src/keccak.hpp/.cpp`, `src/crypto.hpp/.cpp`  
> **Auteur** : Martial Zinsou

---

## 🔐 SHA-256 (FIPS 180-4)

### Implémentation
- **Fichiers** : `src/sha256.hpp/.cpp`
- **Classe** : `SHA256` (incrémental : `update` / `digest`) + `sha256(bytes)` stateless
- **Constantes** : 8 mots d'init (racines carrées premiers 8 nombres premiers), 64 constantes de tour K[64] (racines cubiques premiers 64 nombres premiers)
- **Padding** : 1 bit '1', zéros, longueur 64 bits big-endian
- **Bloc** : 512 bits (64 octets), 64 tours
- **Sortie** : 256 bits (32 octets, big-endian)

### Vecteurs de test (FIPS 180-4)
```
SHA256("")      = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
SHA256("abc")   = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
SHA256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")
                = 248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1
```

---

## 🔑 Keccak-256 / SHA3-256 (FIPS 202)

### Implémentation
- **Fichiers** : `src/keccak.hpp/.cpp`
- **Classes** : `Keccak256` (dom=0x01 défaut, dom=0x06 pour SHA3-256), `keccak_f1600(uint64_t[25])` cœur exposé
- **Architecture éponge** :
  - État : 1600 bits = 25×64 bits (5×5 lanes)
  - Taux (rate) : 1088 bits = 136 octets (Keccak-256)
  - Capacité : 512 bits (sécurité 256 bits)
- **Permutation Keccak-f[1600]** (24 tours) :
  1. **θ (theta)** : parité colonnes → XOR lignes
  2. **ρ (rho)** : rotation bit à bit par constantes RHO[5][5]
  3. **π (pi)** : permutation lanes (x,y) → (y, 2x+3y)
  4. **χ (chi)** : non-linéaire par ligne `a = a ⊕ (¬b ∧ c)`
  5. **ι (iota)** : XOR constante de tour RC[round] sur lane (0,0)
- **Padding multi-rate** : `dom | 0x00* | 0x80` (dom=0x01 Keccak, 0x06 SHA3)
- **Sortie** : 32 premiers octets de l'état (little-endian lanes → big-endian bytes)

### Vecteur de test XKCP (permutation)
```
keccak_f1600(0) = f1258f7940e1dde7... (premiers 8 octets)
```

### Vecteurs Keccak-256 (Ethereum)
```
Keccak256("")   = c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470
Keccak256("abc")= 4e03657aea45a94fc7d47ba826c8d667c0d1e6e33a64a036ec44f58fa12d6c45
```

---

## 📐 secp256k1 (Elliptic Curve)

### Paramètres (SEC 2 / Standards for Efficient Cryptography)
```
p  = FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFE FFFFFC2F
   = 2^256 - 2^32 - 977

n  = FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFE BAAEDCE6 AF48A03B BFD25E8C D0364141
   (ordre du sous-groupe, premier)

Gx = 79BE667E F9DCBBAC 55A06295 CE870B07 029BFCDB 2DCE28D9 59F2815B 16F81798
Gy = 483ADA77 26A3C465 5DA4FBFC 0E1108A8 FD17B448 A6855419 9C47D08F FB10D4B8
```

Équation : `y² = x³ + 7 (mod p)` (courbe Koblitz, a=0, b=7)

### Représentation des points
- **API publique** : Coordonnées affines `(x,y)` (`PublicKey` struct)
- **Interne** : Coordonnées de Jacobien `(X,Y,Z)` pour éviter inversions
  - Affine → Jacobien : `(x,y) → (x,y,1)`
  - Jacobien → Affine : `x = X/Z², y = Y/Z³` (une seule inversion à la fin)

### Formules (Jacobien)
- **Doublage** (2P) : formule "dbl-2007-bl" (8M + 3S + 7A)
- **Addition** (P+Q, P≠Q) : formule "add-2007-bl" (11M + 5S + 9A)
- **Addition mixte** (Jacobien + Affine) : optimisée pour `ecMulG`

### Multiplication scalaire
- **Algorithme** : Échelle binaire gauche→droite (MSB→LSB)
- **Fenêtre** : Non fenêtrée (simple, constant-time pour bits du scalaire)
- **Précalcul** : Aucun (MVP) — optimisable avec table 2⁴ points

---

## ✍️ ECDSA (RFC 6979 Déterministe)

### Signature
Données : message hash `z` (32 octets), clé privée `d` (0 < d < n)

1. **Génération k (RFC 6979)** :
   - HMAC-SHA256 DRBG avec clé = `d`, message = `z`
   - Boucle jusqu'à `0 < k < n`
   - `k` déterministe pour même `(d,z)` — pas d'entropie runtime

2. **Calcul signature** :
   - `R = k * G` ; `r = R.x mod n` (si r=0 → retry)
   - `s = k⁻¹(z + r*d) mod n`
   - **Low-s** : si `s > n/2` → `s = n - s` (BIP-62, malléabilité)
   - `recid = (R.y & 1) | ((R.x >= n) ? 2 : 0)` (parité y + débordement x)

### Vérification
Données : `z`, `(r,s)`, clé publique `Q`

1. `w = s⁻¹ mod n`
3. `u1 = z*w mod n`, `u2 = r*w mod n`
4. `R = u1*G + u2*Q` (addition Jacobien)
5. Valide si `R.x mod n == r`

### Récupération (ecrecover)
Données : `z`, `(r,s,recid)`

1. `x = r + (recid & 2 ? n : 0)` (gérer dépassement x ≥ n)
2. `y² = x³ + 7 mod p` → `y = sqrt(y²)` via `y = y²^((p+1)/4) mod p`
   - Choisir `y` tel que `y & 1 == recid & 1` (parité)
3. `R = (x,y)` ; `e = z` (entier)
4. `Q = r⁻¹(s*R - e*G)` → clé publique récupérée

---

## 🏠 Adresses Nstrike (Style Ethereum)

```
pubkey = 0x04 || x(32) || y(32)  (65 octets, non-compressed)
address = keccak256(pubkey)[12..31]  (20 derniers octets)
```

---

## 📂 Fichiers Sources

| Fichier | Description |
|---------|-------------|
| `sha256.hpp/.cpp` | SHA-256 (classe + stateless) |
| `keccak.hpp/.cpp` | Keccak-256, SHA3-256, Keccak-f[1600] |
| `crypto.hpp/.cpp` | secp256k1, ECDSA, RFC6979, ecrecover, clés, adresses, HMAC-SHA256 |

---

## 🔗 Références

- [FIPS 180-4](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf) — SHA-256
- [FIPS 202](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.202.pdf) — SHA-3/Keccak
- [SEC 2](https://www.secg.org/sec2-v2.pdf) — Paramètres secp256k1
- [RFC 6979](https://datatracker.ietf.org/doc/html/rfc6979) — ECDSA déterministe
- [XKCP](https://github.com/XKCP/XKCP) — Vecteurs Keccak officiels
- [Ethereum Yellow Paper](https://ethereum.github.io/yellowpaper/paper.pdf) — Modèle de référence