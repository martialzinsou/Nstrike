// ------------------------------------------------------------------
// Nstrike — JSON minimal (parse + sérialisation) pour le serveur RPC.
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include "common.hpp"

namespace nstrike {

// JSON minimal (RFC 8259, sous-ensemble raisonnable) pour le RPC.
// Valeurs : null, booléen, entier (64 bits), nombre (double), chaîne,
// tableau, objet. Le parsing est tolérant (espaces, \uXXXX, échappements).
class Json {
public:
    enum class Type { Null, Bool, Int, Double, String, Array, Object };

    Json() : v_(nullptr) {}
    Json(std::nullptr_t) : v_(nullptr) {}
    Json(bool b) : v_(b) {}
    Json(int v) : v_(static_cast<int64_t>(v)) {}
    Json(int64_t v) : v_(v) {}
    Json(double d) : v_(d) {}
    Json(const std::string& s) : v_(s) {}
    Json(const char* s) : v_(std::string(s)) {}
    Json(const bytes& b) : v_(toHex(b)) {}

    Type type() const;
    bool isNull() const { return type() == Type::Null; }
    bool isBool() const { return type() == Type::Bool; }
    bool isInt() const { return type() == Type::Int; }
    bool isString() const { return type() == Type::String; }
    bool isArray() const { return type() == Type::Array; }
    bool isObject() const { return type() == Type::Object; }

    bool asBool(bool d = false) const;
    int64_t asInt(int64_t d = 0) const;
    double asDouble(double d = 0) const;
    const std::string& asString() const;
    const std::vector<Json>& asArray() const;
    const std::map<std::string, Json>& asObject() const;

    // accès objet ("" si absent)
    const Json& get(const std::string& key) const;
    bool has(const std::string& key) const;

    // construction
    static Json array(std::vector<Json> a);
    static Json object(std::map<std::string, Json> o);
    void push(Json v);
    void set(const std::string& k, Json v);

    // sérialisation (compacte) / parsing
    std::string dump() const;
    static Json parse(const std::string& s);      // lève std::runtime_error
    static bool tryParse(const std::string& s, Json& out);

private:
    std::variant<std::nullptr_t, bool, int64_t, double, std::string,
                 std::vector<Json>, std::map<std::string, Json>>
        v_;
    void dumpInto(std::string& out) const;
    static Json parseValue(const char*& p, const char* end);
    static void skipWs(const char*& p, const char* end);
};

} // namespace nstrike