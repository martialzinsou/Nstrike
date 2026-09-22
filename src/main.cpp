// ------------------------------------------------------------------
// Nstrike — point d'entrée CLI (portefeuille, minage, RPC, jetons).
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#include "common.hpp"
#include "crypto.hpp"
#include "json.hpp"
#include "state.hpp"
#include "tx.hpp"
#include "vm.hpp"
#include "chain.hpp"
#include "rpc.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

namespace nstrike {

// ---------------------------------------------------------------
// Portefeuille simple : fichier JSON { "keys": [ {privHex, addrHex}, ... ] }
// ---------------------------------------------------------------
static fix20 addrFromHex(const std::string& s) {
    fix20 a{};
    bytes b; if (!fromHex(s, b) || b.size() != 20) throw std::runtime_error("adresse invalide");
    std::copy(b.begin(), b.end(), a.begin());
    return a;
}
static u256 u256FromHex(const std::string& s) { return u256::fromHex(s); }

struct Wallet {
    struct Entry { fix32 priv; fix20 addr; };
    std::vector<Entry> keys;
    std::string path;

    Wallet() : path(walletPath()) {}

    static std::string walletPath() {
        const char* home = std::getenv("HOME");
        return (home ? std::string(home) : ".") + "/.nstrike/wallet.json";
    }

    void load() {
        std::ifstream in(path);
        if (!in) return;
        std::stringstream ss; ss << in.rdbuf();
        Json j = Json::parse(ss.str());
        for (const Json& e : j.get("keys").asArray()) {
            Entry en;
            std::string ph = e.get("priv").asString();
            bytes pb; fromHex(ph, pb);
            if (pb.size() == 32) {
                std::copy(pb.begin(), pb.end(), en.priv.begin());
                en.addr = e.has("addr") ? addrFromHex(e.get("addr").asString())
                                         : addressFromPrivate(bytes(en.priv.begin(), en.priv.end()));
                keys.push_back(en);
            }
        }
    }

    void save() const {
        std::filesystem::create_directories(path.substr(0, path.rfind('/')));
        Json j = Json::object({});
        std::vector<Json> arr;
        for (const auto& e : keys) {
            Json o = Json::object({});
            o.set("priv", Json(toHex(e.priv)));
            o.set("addr", Json(toHex(e.addr)));
            arr.push_back(o);
        }
        j.set("keys", Json::array(arr));
        std::ofstream out(path);
        out << j.dump();
    }

    fix20 newAccount() {
        bytes priv = generatePrivate();
        fix20 addr = addressFromPrivate(priv);
        fix32 pv{};
        std::copy(priv.begin(), priv.end(), pv.begin());
        keys.push_back({pv, addr});
        save();
        return addr;
    }

    const Entry* findByAddr(const fix20& a) const {
        for (const auto& e : keys) if (e.addr == a) return &e;
        return nullptr;
    }
};

// ---------------------------------------------------------------
// Helpers CLI
// ---------------------------------------------------------------
void printUsage(const char* prog) {
    std::cerr <<
        "Usage: " << prog << " <commande> [args...]\n"
        "Commandes :\n"
        "  account new                crée un nouveau compte\n"
        "  account list               liste les comptes\n"
        "  address <privHex>          calcule l'adresse depuis la clé privée\n"
        "  balance <addr>             solde NST\n"
        "  send <privHex> <to> <amt> [gasPrice]\n"
        "  token create <privHex> <name> <symbol> <decimals> <supply>\n"
        "  token transfer <privHex> <tokenAddr> <to> <amt>\n"
        "  token balance <tokenAddr> <addr>\n"
        "  mine <minerAddr> [count]   minage de blocs\n"
        "  chain                      état de la chaîne\n"
        "  rpc                        serveur JSON-RPC sur stdio\n";
}

// Helpers d'affichage
static std::string toHexStr(const fix32& h) { return "0x" + toHex(h); }
static std::string toHexStr(const fix20& h) { return "0x" + toHex(h); }
static std::string toHexStr(const u256& v) { return v.toHexStr(); }

// ---------------------------------------------------------------
// Commandes
// ---------------------------------------------------------------
int cmdAccount(Chain& ch, Wallet& w, int argc, char** argv) {
    if (argc < 3) { printUsage(argv[0]); return 1; }
    std::string sub = argv[2];
    if (sub == "new") {
        fix20 a = w.newAccount();
        std::cout << "Nouveau compte : " << toHexStr(a) << "\n";
    } else if (sub == "list") {
        for (size_t i = 0; i < w.keys.size(); ++i)
            std::cout << i << ": " << toHexStr(w.keys[i].addr)
                      << "  (priv: " << toHexStr(w.keys[i].priv) << ")\n";
    } else { printUsage(argv[0]); return 1; }
    return 0;
}

int cmdAddress(Wallet& w, int argc, char** argv) {
    if (argc < 3) { printUsage(argv[0]); return 1; }
    fix20 a = addrFromHex(argv[2]);
    // Si la clé est dans le wallet, afficher l'adresse ; sinon calculer
    const auto* e = w.findByAddr(a);
    if (e) std::cout << "Wallet: " << toHexStr(e->addr) << "\n";
    else {
        bytes priv; fromHex(argv[2], priv);
        fix20 calc = addressFromPrivate(priv);
        std::cout << "Calculée: " << toHexStr(calc) << "\n";
    }
    return 0;
}

int cmdBalance(Chain& ch, int argc, char** argv) {
    if (argc < 3) { printUsage(argv[0]); return 1; }
    fix20 a = addrFromHex(argv[2]);
    u256 bal = ch.currentState().balance(a);
    std::cout << toHexStr(a) << " -> " << toHexStr(bal) << " nwei (" << bal.toDecStr() << ")\n";
    return 0;
}

int cmdSend(Chain& ch, Wallet& w, int argc, char** argv) {
    if (argc < 5) { printUsage(argv[0]); return 1; }
    fix20 from = addrFromHex(argv[2]);
    const auto* e = w.findByAddr(from);
    if (!e) { std::cerr << "Clé privée inconnue pour " << toHexStr(from) << "\n"; return 1; }
    fix20 to = addrFromHex(argv[3]);
    u256 val = u256FromHex(argv[4]);
    u256 gasPrice = (argc >= 6) ? u256FromHex(argv[5]) : u256(1); // 1 nwei défaut
    uint64_t nonce = ch.currentState().nonce(from).l[0];

    Transaction tx;
    tx.nonce = u256(nonce);
    tx.gasPrice = gasPrice;
    tx.gasLimit = u256(50000);
    tx.to = to;
    tx.value = val;
    tx.data.clear();
    tx.sign(bytes(e->priv.begin(), e->priv.end()));

    State probe = ch.currentState();
    if (!ch.runTx(probe, tx)) { std::cerr << "Tx invalide\n"; return 1; }
    ch.addTx(tx);
    std::cout << "Tx envoyée (hash=" << toHexStr(tx.hash()) << ")\n";
    return 0;
}

int cmdToken(Chain& ch, Wallet& w, int argc, char** argv) {
    if (argc < 4) { printUsage(argv[0]); return 1; }
    std::string sub = argv[2];
    if (sub == "create") {
        if (argc < 8) { printUsage(argv[0]); return 1; }
        fix20 creator = addrFromHex(argv[3]);
        const auto* e = w.findByAddr(creator);
        if (!e) { std::cerr << "Créateur inconnu\n"; return 1; }
        std::string name = argv[4];
        std::string symbol = argv[5];
        uint8_t decimals = static_cast<uint8_t>(std::stoi(argv[6]));
        u256 supply = u256FromHex(argv[7]);
        uint64_t nonce = ch.currentState().nonce(creator).l[0];
        fix20 addr = erc20Deploy(ch.state, creator, u256(nonce), name, symbol, decimals, supply);
        ch.state.setNonce(creator, u256(nonce + 1));
        std::cout << "Token déployé à " << toHexStr(addr) << "\n";
    } else if (sub == "transfer") {
        if (argc < 7) { printUsage(argv[0]); return 1; }
        fix20 from = addrFromHex(argv[3]);
        const auto* e = w.findByAddr(from);
        if (!e) { std::cerr << "Expéditeur inconnu\n"; return 1; }
        fix20 token = addrFromHex(argv[4]);
        fix20 to = addrFromHex(argv[5]);
        u256 amt = u256FromHex(argv[6]);

        // calldata = selector(transfer(address,uint256)) + to(32) + amt(32)
        fix32 sel = abiSelector("transfer(address,uint256)");
        bytes data(4 + 32 + 32);
        std::copy(sel.begin(), sel.begin() + 4, data.begin());
        fix32 tw = u256::toBytes(u256::fromBytes(to.data(), to.size()));
        std::copy(tw.begin(), tw.end(), data.begin() + 4);
        fix32 aw = u256::toBytes(amt);
        std::copy(aw.begin(), aw.end(), data.begin() + 36);

        uint64_t nonce = ch.currentState().nonce(from).l[0];
        Transaction tx;
        tx.nonce = u256(nonce);
        tx.gasPrice = u256(1);
        tx.gasLimit = u256(100000);
        tx.to = token;
        tx.value = u256(0);
        tx.data = data;
        tx.sign(bytes(e->priv.begin(), e->priv.end()));

        if (!ch.addTx(tx)) { std::cerr << "Tx token invalide\n"; return 1; }
        std::cout << "Transfer tx envoyée (hash=" << toHexStr(tx.hash()) << ")\n";
    } else if (sub == "balance") {
        if (argc < 5) { printUsage(argv[0]); return 1; }
        fix20 token = addrFromHex(argv[3]);
        fix20 addr = addrFromHex(argv[4]);
        // call balanceOf(address)
        fix32 sel = abiSelector("balanceOf(address)");
        bytes data(4 + 32);
        std::copy(sel.begin(), sel.begin() + 4, data.begin());
        fix32 aw = u256::toBytes(u256::fromBytes(addr.data(), addr.size()));
        std::copy(aw.begin(), aw.end(), data.begin() + 4);

        CallContext ctx;
        ctx.self = token;
        ctx.origin = addr;
        ctx.caller = addr;
        ctx.callvalue = u256(0);
        ctx.calldata = data;
        State probe = ch.currentState();
        VMResult r = vmExecute(probe, ctx, 50000);
        if (!r.ok || r.output.size() < 32) { std::cerr << "Échec balanceOf\n"; return 1; }
        fix32 ow = resultWord(r.output, 0);
        std::cout << "Balance " << toHexStr(addr) << " sur " << toHexStr(token)
                  << " = " << toHexStr(u256::fromBytes(ow)) << "\n";
    } else { printUsage(argv[0]); return 1; }
    return 0;
}

int cmdMine(Chain& ch, int argc, char** argv) {
    if (argc < 3) { printUsage(argv[0]); return 1; }
    fix20 miner = addrFromHex(argv[2]);
    size_t count = (argc >= 4) ? std::stoull(argv[3]) : 1;
    uint64_t now = time(nullptr);
    size_t n = ch.mineBlocks(miner, count, now);
    std::cout << n << " bloc(s) miné(s), hauteur = " << ch.blocks.size() - 1 << "\n";
    return 0;
}

int cmdChain(Chain& ch, int argc, char**) {
    std::cout << "Hauteur         : " << ch.blocks.size() << "\n";
    if (!ch.blocks.empty()) {
        const Block& b = ch.blocks.back();
        std::cout << "Dernier bloc    : " << toHexStr(b.header.hash()) << "\n";
        std::cout << "Difficulté      : " << b.header.difficultyBits.l[0] << " bits\n";
        std::cout << "Miner           : " << toHexStr(b.header.miner) << "\n";
    }
    std::cout << "Mempool         : " << ch.mempool.size() << "\n";
    std::cout << "StateRoot       : " << toHexStr(ch.root()) << "\n";
    return 0;
}

int cmdRpc(Chain& ch, int, char**) {
    std::cout << "Serveur JSON-RPC 2.0 sur stdio (Ctrl-D pour quitter)...\n";
    rpcServeStdio(ch);
    return 0;
}

// ---------------------------------------------------------------
// Main
// ---------------------------------------------------------------
int main(int argc, char** argv) {
    if (argc < 2) { printUsage(argv[0]); return 1; }

    Chain chain;
    Wallet wallet;
    wallet.load();

    std::string cmd = argv[1];
    int ret = 1;
    try {
        if (cmd == "account") ret = cmdAccount(chain, wallet, argc, argv);
        else if (cmd == "address") ret = cmdAddress(wallet, argc, argv);
        else if (cmd == "balance") ret = cmdBalance(chain, argc, argv);
        else if (cmd == "send") ret = cmdSend(chain, wallet, argc, argv);
        else if (cmd == "token") ret = cmdToken(chain, wallet, argc, argv);
        else if (cmd == "mine") ret = cmdMine(chain, argc, argv);
        else if (cmd == "chain") ret = cmdChain(chain, argc, argv);
        else if (cmd == "rpc") ret = cmdRpc(chain, argc, argv);
        else { printUsage(argv[0]); ret = 1; }
    } catch (const std::exception& e) {
        std::cerr << "Erreur: " << e.what() << "\n";
        ret = 1;
    }
    return ret;
}

} // namespace nstrike

// main doit être hors namespace pour le linker
int main(int argc, char** argv) {
    if (argc < 2) { nstrike::printUsage(argv[0]); return 1; }

    nstrike::Chain chain;
    nstrike::Wallet wallet;
    wallet.load();

    std::string cmd = argv[1];
    int ret = 1;
    try {
        if (cmd == "account") ret = nstrike::cmdAccount(chain, wallet, argc, argv);
        else if (cmd == "address") ret = nstrike::cmdAddress(wallet, argc, argv);
        else if (cmd == "balance") ret = nstrike::cmdBalance(chain, argc, argv);
        else if (cmd == "send") ret = nstrike::cmdSend(chain, wallet, argc, argv);
        else if (cmd == "token") ret = nstrike::cmdToken(chain, wallet, argc, argv);
        else if (cmd == "mine") ret = nstrike::cmdMine(chain, argc, argv);
        else if (cmd == "chain") ret = nstrike::cmdChain(chain, argc, argv);
        else if (cmd == "rpc") ret = nstrike::cmdRpc(chain, argc, argv);
        else { nstrike::printUsage(argv[0]); ret = 1; }
    } catch (const std::exception& e) {
        std::cerr << "Erreur: " << e.what() << "\n";
        ret = 1;
    }
    return ret;
}