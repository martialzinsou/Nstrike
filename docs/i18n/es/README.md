# Nstrike — Cadena de bloques ligera (MVP)

> **Autor** : Martial Zinsou  
> **Versión** : 0.1.0 (MVP)  
> **Año** : 2026  
> **Licencia** : MIT  
> **Sitio** : https://github.com/martialzinsou/Nstrike

---

## 📋 Tabla de Contenidos

1. [Vista general del proyecto](#vista-general-del-proyecto)
2. [Instalación y Uso](#instalación-y-uso)
3. [Comandos CLI](#comandos-cli)
4. [Compilando el Proyecto](#compilando-el-proyecto)
5. [Pruebas](#pruebas)
6. [Estructura del Proyecto](#estructura-del-proyecto)

---

## 🎯 Visión general del proyecto

**Nstrike** es una implementación de cadena de bloques ligera (MVP) realizada **entiremente desde cero en C++17**, con **ninguna dependencia externa**. El proyecto busca proporcionar una base técnica sólida para una cadena de bloques privada/consorcio con características esenciales tipo Ethereum:

- **Consenso Proof-of-Work (PoW)** con ceros de cabeza
- **Consenso de bloques** con ajuste de dificultad
- **Cuentas** (cuentas, nonce, saldo)
- **Transacciones** (firma ECDSA, gas)
- **Mini-VM (NVM)** para ejecución de contratos
- **Template ERC-20** (token estándar)
- **JSON-RPC** (interfaz 2.0)
- **CLI** (interfaz de línea de comandos)

---

## 📦 Arquitectura Técnica

```
+---------------------+     +-----------------+     +-----------------+
|     CLI (main.cpp)  | --> | RPC (rpc.cpp)     | --> | Cadena (core) |
+---------------------+     +-----------------+     +-----------------+
         |                       |                       |
         |   JSON-RPC       |               |   State (state.cpp)
         |                   +--> chain.cpp (blocks, PoW)
         +--> tokens/tokens.cpp (Template ERC-20)
              +--> vm.cpp (NVM VM + assembler)
                    +--> crypto.cpp (primitives crypto)
                          +--> sha256.hpp/.cpp
                          +--> keccak.hpp/.cpp
```

### Restricciones Técnicas Clave

- **Idioma** : C++17 únicamente
- **Dependencias** : Cero dependencias externas (todo from scratch)
- **Seguridad** : Audits crypto internos, sin dependencias de terceros
- **Rendimiento** : PoW ligero para entornos mobile/embedded
- **Documentación** : Documentación completa (Doxygen, Wiki, README)

---

## 📦 Instalación y Uso

```bash
# Clonar el repositorio
git clone https://github.com/martialzinsou/Nstrike.git
cd Nstrike

# Compilar
make            # build/nstrike (CLI)
make test       # 32 verificaciones (0 fracaso)

# Uso CLI
./build/nstrike account new       # Crear una nueva cuenta
./build/nstrike balance <addr>    # Ver saldo
./build/nstrike send <priv> <to> <amt>  # Efectuar un transfer
./build/nstrike mine <addr> [count]  # Minar bloques
./build/nstrike rpc               # Servidor JSON-RPC

# Desplegar token ERC-20
./build/nstrike token create <priv> "Name" "SYM" 18 <supply>

# Ver estado de la cadena
./build/nstrike chain

# Servidor JSON-RPC
./build/nstrike rpc
# (luego enviar requests JSON-RPC por stdin)
```

---

## 🔧 Compilando el Proyecto

```bash
make            # Compilar el proyecto
make test       # Ejecutar 32 verificaciones
make clean      # Limpiar artefactos de compilación
```

### Requisitos de Compilación

- **Compilador** : Clang++ 17 con `-std=c++17 -O2`
- **SO** : Probado en macOS ARM64 / x86_64
- **Memoria** : Mínimo 256 MB RAM (para ejecución VM)
- **Almacenamiento** : Solo en memoria (MVP, persistencia en disco no incluida)

---

## 📚 Documentación

- **Wiki** : 10 páginas en GitHub Wiki (Markdown)
- **Doxygen** : Documentación completa en todos los `.hpp`/`.cpp`
- **README** : Este archivo
- **Tests** : 32 tests unitarios en `tests/test_main.cpp`

---

## 🔗 Enlaces Útiles

- [Repositorio GitHub](https://github.com/martialzinsou/Nstrike)
- [Informe de Tests](https://github.com/martialzinsou/Nstrike/actions)
- [Paper Amarillo Ethereum](https://ethereum.github.io/yellowpaper/paper.pdf)
- [FIPS 180-4 (SHA-256)](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf)
- [FIPS 202 (SHA-3/Keccak)](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.202.pdf)
- [RFC 6979 (ECDSA determinista)](https://datatracker.ietf.org/doc/html/rfc6979)

---

## 📄 Licencia

Este proyecto está licenciado bajo **MIT**. Ver el archivo `LICENSE` para detalles.

---

## 📞 Contacto

- **GitHub** : https://github.com/martialzinsou/Nstrike
- **Proyecto** : https://github.com/martialzinsou/Nstrike
- **Autor** : Martial Zinsou

---

*Documentación generada el [Fecha Actual]*
*Proyecto open-source bajo licencia MIT - libre para usar, modificar y redistribuir.*