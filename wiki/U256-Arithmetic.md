<meta name="google-site-verification" content="NXMEj6ESFJFiBGZP43G1v36Kka9lC_4Wfum2OMRZTBU" />
# U256 Arithmetic — Entier 256 bits & Arithmétique Modulaire

> **Module** : `src/common.hpp/.cpp` — classe `u256`

---

## 📘 About

**Description** : Arithmétique modulo 2^256 from scratch utilisant la représentation sur 4×uint64_t little-endian. Opérations supportées : addMod, subMod, mulMod (Montgomery REDC), powMod, invMod (Fermat), divmod exact. Vecteurs de test FIPS et XKCP validés.

**Tags** : #u256 #arithmetic #montgomery #modular #math #256bit #from-scratch  
> **Auteur** : Martial Zinsou

---

## 🔢 Représentation

```cpp
class u256 {
    std::array<uint64_t, 4> l{0,0,0,0};  // little-endian: l[0]=bits 0..63, l[3]=bits 192..255
};
```

- **Valeur** : `Σ l[i] * 2^{64*i}` (non signé, 256 bits)
- **Ordre** : Little-endian interne (optimisé pour arithmétique)
- **I/O** : Big-endian (32 octets) pour hashes, adresses, sérialisation

---

## ⚙️ Opérations de Base

### Comparaisons & Tests
| Méthode | Description |
|---------|-------------|
| `cmp(o)` | -1 / 0 / 1 (ordre non signé) |
| `isZero()` | `l[0]\|l[1]\|l[2]\|l[3] == 0` |
| `bit(i)` | Bit i (0=LSB), renvoie 0/1 |
| `shlBits(n)` / `shrBits(n)` | Décalage gauche/droite (0..255, ≥256 → 0) |

### Constantes
- `one()` → `1`
- `max()` → `2^256-1` (tous bits à 1)

---

## ➕ Arithmétique Classique (Modulo 2^256)

> ⚠️ **Ces méthodes ne réduisent PAS modulo un argument** — elles opèrent modulo 2^256 (wrap-around EVM-style).

```cpp
u256& addMod(const u256& b, const u256& mod);  // *this += b (wrap 2^256)
u256& subMod(const u256& b, const u256& mod);  // *this -= b (wrap + add mod si underflow)
```

> ⚠️ L'argument `mod` est **ignoré** dans l'implémentation actuelle (compatibilité API).  
> Pour arithmétique modulaire correcte → utiliser `mulMod`, `powMod`, `invMod`.

---

## 🏎️ Arithmétique de Montgomery (Forme R = 2^256)

### Principe
Pour un module impair `m` (p ou n de secp256k1) :
- **Forme Montgomery** : `a_R = a * R mod m` où `R = 2^256`
- **Multiplication** : `montMul(a_R, b_R) = a * b * R⁻¹ mod m` (reste en forme Montgomery)
- **Conversion** : `toMont(a) = a * R² mod m`, `fromMont(a_R) = a_R * R⁻¹ = a mod m`

### Constantes pré-calculées
- `R² mod m` : via `pow2mod(512, m)` (doublement 512× avec bit haut explicite)
- `m' = -m⁻¹ mod 2^64` (inverse de Newton-Raphson 6 itérations)

### Algorithme REDC (CIOS — Coarsely Integrated Operand Scanning)
```
Input: a,b en forme Montgomery (a_R, b_R), module m
1. Produit 512 bits t = a_R * b_R (école classique, retenue propagée)
2. Pour i = 0..3:
     k = t[i] * m' mod 2^64
     t = t + k * m * 2^{64*i}   (addition colonne par colonne avec retenue)
3. Résultat = t[4..7] (mots hauts)
4. Repli final : while (t[8]>0 || res >= m) res -= m
```

### API Montgomery
```cpp
static u256 toMont(const u256& a, const u256& mod);      // a → a*R mod m
static u256 fromMont(const u256& a, const u256& mod);    // a_R → a
static u256 montMul(const u256& a, const u256& b, const u256& mod); // a_R * b_R → (ab)_R
static u256 powMod(const u256& a, const u256& e, const u256& mod);  // a^e mod m
static u256 invMod(const u256& a, const u256& mod);      // a^{-1} = a^{m-2} mod m
```

---

## 🔁 Exponentiation Modulaire (`powMod`)

### Algorithme (Échelle binaire gauche→droite)
```
Input: a (base normale), e (exposant), m (module)
1. a_R = toMont(a, m)
2. acc_R = toMont(1, m)  // identité en forme Montgomery
3. Pour i = 255 downto 0:
       acc_R = montMul(acc_R, acc_R, m)  // carré
       if e.bit(i): acc_R = montMul(acc_R, a_R, m)  // × base
4. Retour fromMont(acc_R, m)
```
- **Complexité** : 255 carrés + ~128 multiplications (bits 1)
- **Constant-time** : parcours fixe 256 itérations (pas de branchement secret)

---

## 🔄 Inversion Modulaire (`invMod`)

### Petit théorème de Fermat
Pour `m` premier (p ou n de secp256k1) :
```
a^{-1} ≡ a^{m-2} (mod m)
```
Implémenté via `powMod(a, m-2, m)` — utilise l'échelle Montgomery.

---

## ➗ Division Exacte (`divmod`)

### Algorithme Shift-and-Subtract (256 itérations MSB→LSB)
```
Input: a (dividende), b (diviseur ≠ 0)
Output: q (quotient), r (reste), 0 ≤ r < b

r = 0; q = 0
Pour i = 255 downto 0:
    r <<= 1
    r.bit(0) = a.bit(i)
    if r >= b:
        r -= b
        q.bit(i) = 1
```
- **Complexité** : 256 itérations fixes (constant-time)
- **Utilisation** : Conversion décimale (`toDecStr`), inversion modulaire (non utilisée directement)

---

## 🔁 Produit Modulaire Complet (`mulMod`)

```cpp
static u256 mulMod(const u256& a, const u256& b, const u256& mod);
```
1. `a_R = toMont(a, mod)`
2. `b_R = toMont(b, mod)`
3. `r_R = montMul(a_R, b_R, mod)`
4. `return fromMont(r_R, mod)`

Plus lent que `montMul` direct mais interface simple (entrées/sorties forme normale).

---

## 🔄 Conversion Décimale (`toDecStr`)

### Algorithme (Base 10^19)
```
diviseur = 10^19 (tient dans uint64_t)
while !q.isZero():
    divmod(q, 10^19, q, r)
    groups.push_back(r.l[0])  // reste < 10^19 → tient dans 64 bits
Assemble groupes avec padding zéros à gauche (19 digits par groupe sauf premier)
```
- **Pourquoi 10^19** : Plus grande puissance de 10 < 2^64
- **Complexité** : O(log_{10^19}(value)) itérations

---

## 🎲 Génération Aléatoire

```cpp
bytes randomBytes(size_t n);
```
- Source : `std::random_device` + `mt19937_64`
- 64 bits par appel `mt19937_64()`
- ⚠️ **Non validé pour production** — utiliser source validée (OpenSSL, getrandom)

---

## 📐 Vecteurs de Test (Interne)

| Test | Description |
|------|-------------|
| `u256_basics` | Constructeurs, comparaisons, bit ops, shifts |
| `u256_modular` | `invMod(a)*a=1`, `mulMod(r1,r2)==r3`, `powMod`, `mulMod(p-1,y)` |
| `hex_roundtrip` | `fromHex` ↔ `toHexStr` identité |

---

## 📂 Fichiers

| Fichier | Contenu |
|---------|---------|
| `common.hpp` | Déclaration `u256`, types, hex, aléa |
| `common.cpp` | Implémentation complète (Montgomery, divmod, parsing) |

---

## 🔗 Références

- [Montgomery Multiplication (CIOS)](https://www.microsoft.com/en-us/research/wp-content/uploads/2016/02/montgomery.pdf)
- [Handbook of Applied Cryptography, Ch.14](https://cacr.uwaterloo.ca/hac/)
- [Ethereum Yellow Paper, Appendix F](https://ethereum.github.io/yellowpaper/paper.pdf) — Arithmétique modulaire EVM