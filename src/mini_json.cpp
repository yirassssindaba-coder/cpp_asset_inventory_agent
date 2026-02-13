#include "mini_json.hpp"
#include <sstream>
#include <iomanip>

namespace minijson {

const Value& Value::at(const std::string& k) const {
    auto it = o.find(k);
    if (it == o.end()) throw std::runtime_error("missing key: " + k);
    return it->second;
}
bool Value::has(const std::string& k) const {
    return o.find(k) != o.end();
}

struct Parser {
    const std::string& t;
    size_t i = 0;

    explicit Parser(const std::string& s): t(s) {}

    void ws() { while (i < t.size() && std::isspace((unsigned char)t[i])) i++; }
    char peek() { ws(); return (i < t.size()) ? t[i] : '\0'; }
    char get() { if (i >= t.size()) return '\0'; return t[i++]; }

    void expect(char c) {
        ws();
        if (get() != c) throw std::runtime_error(std::string("expected '") + c + "'");
    }

    Value parse_value() {
        ws();
        char c = peek();
        if (c == '"') return Value::string(parse_string());
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == 't') { consume("true"); return Value::boolean(true); }
        if (c == 'f') { consume("false"); return Value::boolean(false); }
        if (c == 'n') { consume("null"); return Value::nullv(); }
        if (c == '-' || std::isdigit((unsigned char)c)) return Value::number(parse_number());
        throw std::runtime_error("invalid json value");
    }

    void consume(const char* lit) {
        ws();
        for (const char* p=lit; *p; ++p) {
            if (get() != *p) throw std::runtime_error(std::string("expected literal: ") + lit);
        }
    }

    std::string parse_string() {
        ws();
        if (get() != '"') throw std::runtime_error("expected string quote");
        std::string out;
        while (i < t.size()) {
            char c = get();
            if (c == '"') break;
            if (c == '\\') {
                char e = get();
                switch (e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        // minimal \uXXXX support -> store as '?'
                        for (int k=0;k<4;k++) { if (!std::isxdigit((unsigned char)get())) throw std::runtime_error("bad \\u escape"); }
                        out.push_back('?');
                        break;
                    }
                    default: throw std::runtime_error("bad escape");
                }
            } else {
                out.push_back(c);
            }
        }
        return out;
    }

    double parse_number() {
        ws();
        size_t start = i;
        if (peek() == '-') get();
        while (std::isdigit((unsigned char)peek())) get();
        if (peek() == '.') { get(); while (std::isdigit((unsigned char)peek())) get(); }
        if (peek() == 'e' || peek() == 'E') {
            get();
            if (peek() == '+' || peek() == '-') get();
            while (std::isdigit((unsigned char)peek())) get();
        }
        std::string sub = t.substr(start, i - start);
        try { return std::stod(sub); } catch (...) { throw std::runtime_error("bad number"); }
    }

    Value parse_array() {
        expect('[');
        std::vector<Value> arr;
        ws();
        if (peek() == ']') { get(); return Value::array(std::move(arr)); }
        while (true) {
            arr.push_back(parse_value());
            ws();
            char c = get();
            if (c == ']') break;
            if (c != ',') throw std::runtime_error("expected , or ]");
        }
        return Value::array(std::move(arr));
    }

    Value parse_object() {
        expect('{');
        std::map<std::string, Value> obj;
        ws();
        if (peek() == '}') { get(); return Value::object(std::move(obj)); }
        while (true) {
            std::string key = parse_string();
            ws();
            expect(':');
            Value val = parse_value();
            obj.emplace(std::move(key), std::move(val));
            ws();
            char c = get();
            if (c == '}') break;
            if (c != ',') throw std::runtime_error("expected , or }");
        }
        return Value::object(std::move(obj));
    }
};

Value parse(const std::string& text) {
    Parser p(text);
    Value v = p.parse_value();
    p.ws();
    if (p.i != text.size()) {
        // allow trailing whitespace
        for (; p.i<text.size(); ++p.i) if (!std::isspace((unsigned char)text[p.i])) throw std::runtime_error("trailing data");
    }
    return v;
}

static std::string esc(const std::string& s) {
    std::ostringstream o;
    for (char c: s) {
        switch (c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) o << "?";
                else o << c;
        }
    }
    return o.str();
}

static void indent(std::ostringstream& o, int n) { for (int i=0;i<n;i++) o << ' '; }

std::string stringify(const Value& v, bool pretty, int indent_level) {
    std::ostringstream o;
    switch (v.type) {
        case Value::Type::Null: o << "null"; break;
        case Value::Type::Bool: o << (v.b ? "true" : "false"); break;
        case Value::Type::Number: {
            o << std::setprecision(15) << v.num;
            break;
        }
        case Value::Type::String:
            o << "\"" << esc(v.s) << "\"";
            break;
        case Value::Type::Array: {
            o << "[";
            if (!v.a.empty()) {
                if (pretty) o << "\n";
                for (size_t i=0;i<v.a.size(); ++i) {
                    if (pretty) indent(o, indent_level + 2);
                    o << stringify(v.a[i], pretty, indent_level + 2);
                    if (i + 1 < v.a.size()) o << ",";
                    if (pretty) o << "\n";
                }
                if (pretty) indent(o, indent_level);
            }
            o << "]";
            break;
        }
        case Value::Type::Object: {
            o << "{";
            if (!v.o.empty()) {
                if (pretty) o << "\n";
                size_t n=0;
                for (const auto& kv: v.o) {
                    if (pretty) indent(o, indent_level + 2);
                    o << "\"" << esc(kv.first) << "\":";
                    if (pretty) o << " ";
                    o << stringify(kv.second, pretty, indent_level + 2);
                    if (++n < v.o.size()) o << ",";
                    if (pretty) o << "\n";
                }
                if (pretty) indent(o, indent_level);
            }
            o << "}";
            break;
        }
    }
    return o.str();
}

} // namespace minijson
