#pragma once
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <cctype>

namespace minijson {

struct Value {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;

    bool b = false;
    double num = 0.0;
    std::string s;
    std::vector<Value> a;
    std::map<std::string, Value> o;

    static Value nullv() { return Value(); }
    static Value boolean(bool v) { Value x; x.type = Type::Bool; x.b = v; return x; }
    static Value number(double v) { Value x; x.type = Type::Number; x.num = v; return x; }
    static Value string(std::string v) { Value x; x.type = Type::String; x.s = std::move(v); return x; }
    static Value array(std::vector<Value> v) { Value x; x.type = Type::Array; x.a = std::move(v); return x; }
    static Value object(std::map<std::string, Value> v) { Value x; x.type = Type::Object; x.o = std::move(v); return x; }

    bool is_null() const { return type == Type::Null; }
    bool is_bool() const { return type == Type::Bool; }
    bool is_number() const { return type == Type::Number; }
    bool is_string() const { return type == Type::String; }
    bool is_array() const { return type == Type::Array; }
    bool is_object() const { return type == Type::Object; }

    const Value& at(const std::string& k) const;
    bool has(const std::string& k) const;
};

Value parse(const std::string& text);
std::string stringify(const Value& v, bool pretty=false, int indent=0);

} // namespace minijson
