// ------------------------------------------------------------------
// Nstrike — implémentation du dispatcher JSON-RPC 2.0.
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#include "rpc.hpp"

#include <cstdio>
#include <string>

#include "keccak.hpp"
#include "crypto.hpp"

namespace nstrike {

namespace {
// réponse succès
Json ok(Json result, const Json& id) {
    Json r = Json::object({});
    r.set("jsonrpc", Json("2.0"));
    r.set("result", std::move(result));
    r.set("id", id);
    return r;
}
// réponse erreur
Json err(int code, const std::string& msg, const Json& id) {
    Json e = Json::object({});
    e.set("code", Json(static_cast<int64_t>(code)));
    e.set("message", Json(msg));
    Json r = Json::object({});
    r.set("jsonrpc", Json("2.0"));
    r.set("error", std::move(e));
    r.set("id", id);
    return r;
}

fix20 addrFromHex(const std::string& s) {
    fix20 a{};
    bytes b;
    if (!fromHex(s, b) || b.size() != 20) throw std::runtime_error("adresse invalide");
    std::copy(b.begin(), b.end(), a.begin());
    return a;
}

u256 u256FromHex(const std::string& s) {
    return u256::fromHex(s);
}

std::string toHexStr(const fix32& h) { return "0x" + toHex(h); }
std::string toHexStr(const fix20& h) { return "0x" + toHex(h); }
std::string toHexStr(const u256& v) { return v.toHexStr(); }
} // namespace

Json rpcHandle(const Json& req, Chain& chain) {
    Json id = req.get("id");
    if (!req.has("method")) return err(-32600, "missing method", id);
    std::string method = req.get("method").asString();

    if (method == "getbalance") {
        fix20 addr = addrFromHex(req.get("params").get("address").asString());
        u256 bal = chain.currentState().balance(addr);
        return ok(Json(toHexStr(bal)), id);
    }
    if (method == "sendtx") {
        std::string txHex = req.get("params").get("tx").asString();
        Transaction tx = Transaction::fromHex(txHex);
        State probe = chain.currentState();
        uint64_t used = 0;
        if (!chain.runTx(probe, tx, &used)) return err(-32000, "tx invalide", id);
        chain.addTx(tx); // met en mempool
        return ok(Json(toHexStr(tx.hash())), id);
    }
    if (method == "getblock") {
        uint64_t h = static_cast<uint64_t>(req.get("params").get("height").asInt());
        if (h >= chain.blocks.size()) return err(-32602, "bloc non trouvé", id);
        const Block& b = chain.blocks[h];
        Json r = Json::object({});
        r.set("hash", Json(toHexStr(b.header.hash())));
        r.set("height", Json(static_cast<int64_t>(b.header.height)));
        r.set("timestamp", Json(static_cast<int64_t>(b.header.timestamp)));
        r.set("miner", Json(toHexStr(b.header.miner)));
        r.set("txCount", Json(static_cast<int64_t>(b.txs.size())));
        return ok(r, id);
    }
    if (method == "getblockcount") {
        return ok(Json(static_cast<int64_t>(chain.blocks.size())), id);
    }
    if (method == "getgasprice") {
        return ok(Json("0x" + u256(1).toHexStr()), id); // 1 nwei suggéré
    }
    if (method == "mine") {
        Json params = req.get("params");
        std::string minerHex = params.get("miner").asString();
        uint64_t count = params.has("count") ? static_cast<uint64_t>(params.get("count").asInt()) : 1;
        fix20 miner = addrFromHex(minerHex);
        uint64_t now = time(nullptr);
        size_t mined = chain.mineBlocks(miner, count, now);
        Json r = Json::object({});
        r.set("mined", Json(static_cast<int64_t>(mined)));
        r.set("height", Json(static_cast<int64_t>(chain.blocks.size() - 1)));
        return ok(r, id);
    }
    if (method == "chainstatus") {
        Json r = Json::object({});
        r.set("height", Json(static_cast<int64_t>(chain.blocks.size())));
        r.set("difficulty", Json(toHexStr(chain.blocks.empty() ? u256(Config::DEFAULT_DIFFICULTY_BITS) : chain.blocks.back().header.difficultyBits)));
        r.set("mempool", Json(static_cast<int64_t>(chain.mempool.size())));
        r.set("stateRoot", Json(toHexStr(chain.root())));
        return ok(r, id);
    }
    if (method == "deploytoken") {
        Json p = req.get("params");
        fix20 creator = addrFromHex(p.get("creator").asString());
        uint64_t nonce = static_cast<uint64_t>(p.get("nonce").asInt());
        std::string name = p.get("name").asString();
        std::string symbol = p.get("symbol").asString();
        uint8_t decimals = static_cast<uint8_t>(p.get("decimals").asInt());
        u256 supply = u256FromHex(p.get("supply").asString());
        fix20 addr = erc20Deploy(chain.state, creator, u256(nonce), name, symbol, decimals, supply);
        chain.state.setNonce(creator, u256(nonce + 1));
        return ok(Json(toHexStr(addr)), id);
    }
    if (method == "callcontract") {
        Json p = req.get("params");
        fix20 contract = addrFromHex(p.get("contract").asString());
        std::string dataHex = p.get("data").asString();
        bytes data;
        if (!fromHex(dataHex, data)) return err(-32602, "data invalide", id);
        uint64_t gas = p.has("gas") ? static_cast<uint64_t>(p.get("gas").asInt()) : 50000;
        CallContext ctx;
        ctx.self = contract;
        ctx.origin = p.has("from") ? addrFromHex(p.get("from").asString()) : ADDRESS_ZERO();
        ctx.caller = ctx.origin;
        ctx.callvalue = u256(0);
        ctx.calldata = data;
        State probe = chain.currentState();
        VMResult r = vmExecute(probe, ctx, gas);
        Json res = Json::object({});
        res.set("ok", Json(r.ok));
        res.set("gasUsed", Json(static_cast<int64_t>(r.gasUsed)));
        res.set("output", Json("0x" + toHex(r.output)));
        return ok(res, id);
    }
    return err(-32601, "méthode inconnue: " + method, id);
}

void rpcServeStdio(Chain& chain) {
    char buf[8192];
    while (fgets(buf, sizeof buf, stdin)) {
        std::string line(buf);
        size_t nl = line.find('\n');
        if (nl != std::string::npos) line.resize(nl);
        if (line.empty()) continue;
        Json req, resp;
        if (!Json::tryParse(line, req)) { resp = err(-32700, "parse error", Json(nullptr)); }
        else { resp = rpcHandle(req, chain); }
        fputs(resp.dump().c_str(), stdout);
        fputc('\n', stdout);
        fflush(stdout);
    }
}

} // namespace nstrike