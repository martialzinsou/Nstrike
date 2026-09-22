# Nstrike — Легковесная Блокчейн (MVP)

> **Автор** : Martial Zinsou  
> **Версия** : 0.1.0 (MVP)  
> **Год** : 2026  
> **Лицензия** : MIT  
> **Сайт** : https://github.com/martialzinsou/Nstrike

---

## 📋 Содержание

1. [Обзор проекта](#обзор-проекта)
2. [Установка и использование](#установка-и-استخدام)
3. [CLI-команды](#cli-команды)
4. [Сборка проекта](#сборка-проекта)
5. [Тестирование](#тестирование)
6. [Структура проекта](#структура-проекта)

---

## 🎯 Обзор проекта

**Nstrike** — это легковесная реализация блокчейна **from scratch на C++17**, без внешних зависимостей. Проект направлен на предоставление надежной технической основы для приватного/консорциумного блокчейна с essential Ethereum-features:

- **Proof-of-Work Consensus (PoW)** с нулями впереди
- **Консенс блоков** с ajustment сложности
- **Счета** (счета, nonce, баланс)
- **Транзакции** (подпись ECDSA, gas)
- **Mini-VM (NVM)** для выполнения контрактов
- **ERC-20 Template** (стандартный токен)
- **JSON-RPC** (интерфейс 2.0)
- **CLI** (интерфейс командной строки)

---

## 📦 Техническая архитектура

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

### Ключевые технические ограничения

- **Язык** : C++17 только
- **Зависимости** : Внешних зависимостей нет (всё from scratch)
- **Безопасность** : Внутренние аудиты криптографии, сторонние зависимости нет
- **Производительность** : Легкий PoW для мобильных/встраиваемых сред
- **Документация** : Полная документация (Doxygen, Wiki, README)

---

## 📦 Установка и использование

```bash
# Клонировать репозиторий
git clone https://github.com/martialzinsou/Nstrike.git
cd Nstrike

# Сборка
make            # build/nstrike (CLI)
make test       # 32 проверки (0 неудач)

# Использование CLI
./build/nstrike account new       # Создать новую учетную запись
./build/nstrike balance <addr>    # Проверить баланс
./build/nstrike send <priv> <to> <amt>  # Выполнить перевод
./build/nstrike mine <addr> [count]  # Добыть блоки
./build/nstrike rpc               # JSON-RPC сервер

# Развертывание ERC-20 токена
./build/nstrike token create <priv> "Name" "SYM" 18 <supply>

# Проверить состояние цепи
./build/nstrike chain

# JSON-RPC сервер
./build/nstrike rpc
# (затем отправлять запросы JSON-RPC через stdin)
```

---

## 🔧 Сборка проекта

```bash
make            # Скомпилировать проект
make test       # Запустить 32 проверки
make clean      # Очистить артефакты сборки
```

### Требования к сборке

- **Компилятор** : Clang++ 17 с `-std=c++17 -O2`
- **ОС** : Тестируется на macOS ARM64 / x86_64
- **Память** : Минимум 256 МБ RAM (для выполнения VM)
- **Хранилище** : Только в памяти (MVP, сохранность на диск не включена)

---

## 📚 Документация

- **Wiki** : 10 страниц в GitHub Wiki (Markdown)
- **Doxygen** : Полная документация на все `.hpp`/`.cpp`
- **README** : Этот файл
- **Тесты** : 32 юнит-теста в `tests/test_main.cpp`

---

## 🔗 Полезные ссылки

- [GitHub Repository](https://github.com/martialzinsou/Nstrike)
- [Отчет о тестах](https://github.com/martialzinsou/Nstrike/actions)
- [Ethereum Yellow Paper](https://ethereum.github.io/yellowpaper/paper.pdf)
- [FIPS 180-4 (SHA-256)](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf)
- [FIPS 202 (SHA-3/Keccak)](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.202.pdf)
- [RFC 6979 (Deterministic ECDSA)](https://datatracker.ietf.org/doc/html/rfc6979)

---

## 📄 Лицензия

Проект распространяется под лицензией **MIT**. См. файл `LICENSE` для деталей.

---

## 📞 Контакт

- **GitHub** : https://github.com/martialzinsou/Nstrike
- **Проект** : https://github.com/martialzinsou/Nstrike
- **Автор** : Martial Zinsou

---

*Документация сгенерирована [Текущая дата]*
*Open-source проект под лицензией MIT — свободно для использования, модификации и перераспределения.*