// ------------------------------------------------------------------
// Nstrike — implémentation du parseur/sérialiseur JSON (RFC 8259, sous-
// ensemble : null/bool/int/int64/double/string/array/object, \uXXXX).
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#include "json.hpp"

#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace nstrike {

using namespace std::literals;

// ---------------------------------------------------------------
// Accès typés
// ---------------------------------------------------------------
Json::Type Json::type() const {
    switch (v_.index()) {
        case 1: return Type::Bool;
        case 2: return Type::Int;
        case 3: return Type::Double;
        case 4: return Type::String;
        case 5: return Type::Array;
        case 6: return Type::Object;
        default: return Type::Null;
    }
}

bool Json::asBool(bool d) const { return std::get_if<bool>(&v_) ? std::get<bool>(v_) : d; }
int64_t Json::asInt(int64_t d) const {
    if (auto i = std::get_if<int64_t>(&v_)) return *i;
    return d;
}
double Json::asDouble(double d) const {
    if (auto x = std::get_if<double>(&v_)) return *x;
    if (auto i = std::get_if<int64_t>(&v_)) return static_cast<double>(*i);
    return d;
}
const std::string& Json::asString() const {
    static const std::string empty;
    return std::get_if<std::string>(&v_) ? std::get<std::string>(v_) : empty;
}
const std::vector<Json>& Json::asArray() const {
    static const std::vector<Json> empty;
    return std::get_if<std::vector<Json>>(&v_) ? std::get<std::vector<Json>>(v_) : empty;
}
const std::map<std::string, Json>& Json::asObject() const {
    static const std::map<std::string, Json> empty;
    return std::get_if<std::map<std::string, Json>>(&v_) ? std::get<std::map<std::string, Json>>(v_) : empty;
}

const Json& Json::get(const std::string& key) const {
    static const Json null;
    auto o = std::get_if<std::map<std::string, Json>>(&v_);
    if (!o) return null;
    auto it = o->find(key);
    return it == o->end() ? null : it->second;
}
bool Json::has(const std::string& key) const {
    auto o = std::get_if<std::map<std::string, Json>>(&v_);
    return o && o->count(key);
}

Json Json::array(std::vector<Json> a) {
    Json j;
    j.v_ = std::move(a);
    return j;
}
Json Json::object(std::map<std::string, Json> o) {
    Json j;
    j.v_ = std::move(o);
    return j;
}
void Json::push(Json v) {
    auto& a = std::get<std::vector<Json>>(v_);
    a.push_back(std::move(v));
}
void Json::set(const std::string& k, Json v) {
    std::get<std::map<std::string, Json>>(v_)[k] = std::move(v);
}

// ---------------------------------------------------------------
// Sérialisation
// ---------------------------------------------------------------
static void dumpString(std::string& out, const std::string& s) {
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char b[8];
                    std::snprintf(b, sizeof b, "\\u%04x", c);
                    out += b;
                } else {
                    out.push_back((char)c);
                }
        }
    }
    out.push_back('"');
}

void Json::dumpInto(std::string& out) const {
    switch (type()) {
        case Type::Null: out += "null"; return;
        case Type::Bool: out += std::get<bool>(v_) ? "true" : "false"; return;
        case Type::Int: out += std::to_string(std::get<int64_t>(v_)); return;
        case Type::Double: {
            char b[32];
            double d = std::get<double>(v_);
            std::snprintf(b, sizeof b, "%.17g", d);
            out += b;
            return;
        }
        case Type::String: dumpString(out, std::get<std::string>(v_)); return;
        case Type::Array: {
            out.push_back('[');
            bool first = true;
            for (const auto& e : std::get<std::vector<Json>>(v_)) {
                if (!first) out.push_back(',');
                first = false;
                e.dumpInto(out);
            }
            out.push_back(']');
            return;
        }
        case Type::Object: {
            out.push_back('{');
            bool first = true;
            for (const auto& [k, v] : std::get<std::map<std::string, Json>>(v_)) {
                if (!first) out.push_back(',');
                first = false;
                dumpString(out, k);
                out.push_back(':');
                v.dumpInto(out);
            }
            out.push_back('}');
            return;
        }
    }
}

std::string Json::dump() const {
    std::string s;
    s.reserve(128);
    dumpInto(s);
    return s;
}

// ---------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------
void Json::skipWs(const char*& p, const char* end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
}

static void hex4(const char* p, int& out) {
    out = 0;
    for (int i = 0; i < 4; ++i) {
        char c = p[i];
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = 10 + (c - 'a');
        else if (c >= 'A' && c <= 'F') d = 10 + (c - 'A');
        else throw std::runtime_error("json: \\u invalide");
        out = out * 16 + d;
    }
}

static std::string parseString(const char*& p, const char* end) {
    if (p >= end || *p != '"') throw std::runtime_error("json: chaîne attendue");
    ++p;
    std::string s;
    while (p < end) {
        char c = *p++;
        if (c == '"') return s;
        if (c == '\\') {
            if (p >= end) break;
            char e = *p++;
            switch (e) {
                case '"': s.push_back('"'); break;
                case '\\': s.push_back('\\'); break;
                case '/': s.push_back('/'); break;
                case 'b': s.push_back('\b'); break;
                case 'f': s.push_back('\f'); break;
                case 'n': s.push_back('\n'); break;
                case 'r': s.push_back('\r'); break;
                case 't': s.push_back('\t'); break;
                case 'u': {
                    if (p + 4 > end) throw std::runtime_error("json: \\u tronqué");
                    int cp;
                    hex4(p, cp);
                    p += 4;
                    if (cp < 0x80) {
                        s.push_back((char)cp);
                    } else if (cp < 0x800) {
                        s.push_back((char)(0xC0 | (cp >> 6)));
                        s.push_back((char)(0x80 | (cp & 0x3F)));
                    } else {
                        s.push_back((char)(0xE0 | (cp >> 12)));
                        s.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
                        s.push_back((char)(0x80 | (cp & 0x3F)));
                    }
                    break;
                }
                default: throw std::runtime_error("json: échappement inconnu");
            }
        } else {
            s.push_back(c);
        }
    }
    throw std::runtime_error("json: chaîne non terminée");
}

Json Json::parseValue(const char*& p, const char* end) {
    skipWs(p, end);
    if (p >= end) throw std::runtime_error("json: fin inattendue");
    char c = *p;
    if (c == '{') {
        ++p;
        std::map<std::string, Json> obj;
        skipWs(p, end);
        if (p < end && *p == '}') { ++p; return Json::object(std::move(obj)); }
        while (p < end) {
            skipWs(p, end);
            if (*p != '"') throw std::runtime_error("json: clef objet attendue");
            std::string k = parseString(p, end);
            skipWs(p, end);
            if (p >= end || *p != ':') throw std::runtime_error("json: ':' attendu");
            ++p;
            obj[k] = parseValue(p, end);
            skipWs(p, end);
            if (p < end && *p == ',') { ++p; continue; }
            if (p < end && *p == '}') { ++p; return Json::object(std::move(obj)); }
            throw std::runtime_error("json: objet mal formé");
        }
        throw std::runtime_error("json: objet non terminé");
    }
    if (c == '[') {
        ++p;
        std::vector<Json> arr;
        skipWs(p, end);
        if (p < end && *p == ']') { ++p; return Json::array(std::move(arr)); }
        while (p < end) {
            arr.push_back(parseValue(p, end));
            skipWs(p, end);
            if (p < end && *p == ',') { ++p; continue; }
            if (p < end && *p == ']') { ++p; return Json::array(std::move(arr)); }
            throw std::runtime_error("json: tableau mal formé");
        }
        throw std::runtime_error("json: tableau non terminé");
    }
    if (c == '"') return Json(parseString(p, end));
    if (c == 't') {
        if (end - p >= 4 && std::string(p, 4) == "true") { p += 4; return Json(true); }
        throw std::runtime_error("json: littéral invalide");
    }
    if (c == 'f') {
        if (end - p >= 5 && std::string(p, 5) == "false") { p += 5; return Json(false); }
        throw std::runtime_error("json: littéral invalide");
    }
    if (c == 'n') {
        if (end - p >= 4 && std::string(p, 4) == "null") { p += 4; return Json(nullptr); }
        throw std::runtime_error("json: littéral invalide");
    }
    // nombre
    size_t i = 0;
    if (c == '-') { i = 1; if (p + i >= end) throw std::runtime_error("json: nombre invalide"); }
    bool isDouble = false;
    while (p + i < end) {
        char d = p[i];
        if (d >= '0' && d <= '9') { ++i; continue; }
        if (d == '.' || d == 'e' || d == 'E' || d == '+' || d == '-') { isDouble = true; ++i; continue; }
        break;
    }
    if (i == 0 || (i == 1 && p[0] == '-')) throw std::runtime_error("json: nombre invalide");
    std::string num(p, i);
    p += i;
    if (isDouble) {
        return Json(std::strtod(num.c_str(), nullptr));
    } else {
        errno = 0;
        char* e = nullptr;
        long long v = std::strtoll(num.c_str(), &e, 10);
        if (e && *e == '\0' && errno != ERANGE) return Json(static_cast<int64_t>(v));
        // hors int64 : grand entier -> chaîne décimale (usage : quantités)
        return Json(num);
    }
}

Json Json::parse(const std::string& s) {
    const char* p = s.data();
    const char* end = s.data() + s.size();
    Json v = parseValue(p, end);
    skipWs(p, end);
    if (p != end) throw std::runtime_error("json: contenu superflu après la valeur");
    return v;
}

bool Json::tryParse(const std::string& s, Json& out) {
    try {
        out = parse(s);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace nstrike