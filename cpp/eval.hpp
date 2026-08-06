#pragma once

#include "tokenize.hpp"

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

struct Symbol;
struct SExpr;
struct Lambda;
struct Builtin;
struct Env;
struct Value;

// ---------------------------------------------------------------------------
// Value — the universal runtime value type
// ---------------------------------------------------------------------------

struct Value
{
    enum class Tag
    {
        INT,
        FLOAT,
        BOOL,
        TEXT,
        NIL,
        SYMBOL,
        SEXPR,
        LAMBDA,
        BUILTIN
    };

    Tag tag;

    // tagged union (only one active at a time)
    int64_t int_val;
    double float_val;
    bool bool_val;
    std::string text_val;
    std::shared_ptr<Symbol> symbol_val;
    std::shared_ptr<SExpr> sexpr_val;
    std::shared_ptr<Lambda> lambda_val;
    std::shared_ptr<Builtin> builtin_val;

    // --- constructors ---

    Value() : tag(Tag::NIL), int_val(0) {}

    Value(int64_t v) : tag(Tag::INT), int_val(v) {}
    Value(int v) : tag(Tag::INT), int_val(v) {}
    Value(double v) : tag(Tag::FLOAT), float_val(v) {}
    Value(bool v) : tag(Tag::BOOL), bool_val(v) {}
    Value(const std::string &s) : tag(Tag::TEXT), text_val(s) {}
    Value(const char *s) : tag(Tag::TEXT), text_val(s) {}
    Value(std::nullptr_t) : tag(Tag::NIL), int_val(0) {}

    Value(std::shared_ptr<Symbol> v) : tag(Tag::SYMBOL), symbol_val(std::move(v)) {}
    Value(std::shared_ptr<SExpr> v) : tag(Tag::SEXPR), sexpr_val(std::move(v)) {}
    Value(std::shared_ptr<Lambda> v) : tag(Tag::LAMBDA), lambda_val(std::move(v)) {}
    Value(std::shared_ptr<Builtin> v) : tag(Tag::BUILTIN), builtin_val(std::move(v)) {}

    // --- helpers ---

    bool is_int() const { return tag == Tag::INT; }
    bool is_float() const { return tag == Tag::FLOAT; }
    bool is_bool() const { return tag == Tag::BOOL; }
    bool is_text() const { return tag == Tag::TEXT; }
    bool is_nil() const { return tag == Tag::NIL; }
    bool is_number() const { return tag == Tag::INT || tag == Tag::FLOAT; }
    bool is_symbol() const { return tag == Tag::SYMBOL; }
    bool is_sexpr() const { return tag == Tag::SEXPR; }
    bool is_lambda() const { return tag == Tag::LAMBDA; }
    bool is_builtin() const { return tag == Tag::BUILTIN; }

    // --- truthiness ---
    bool truthy() const
    {
        switch (tag)
        {
        case Tag::NIL:
            return false;
        case Tag::BOOL:
            return bool_val;
        default:
            return true;
        }
    }

    // --- equality ---
    bool eq(const Value &other) const
    {
        if (tag != other.tag)
            return false;
        switch (tag)
        {
        case Tag::INT:
            return int_val == other.int_val;
        case Tag::FLOAT:
            return float_val == other.float_val;
        case Tag::BOOL:
            return bool_val == other.bool_val;
        case Tag::TEXT:
            return text_val == other.text_val;
        case Tag::NIL:
            return true;
        case Tag::SYMBOL:
            return symbol_val == other.symbol_val;
        case Tag::SEXPR:
            return sexpr_val == other.sexpr_val;
        case Tag::LAMBDA:
            return lambda_val == other.lambda_val;
        case Tag::BUILTIN:
            return builtin_val == other.builtin_val;
        }
        return false;
    }
};

// ---------------------------------------------------------------------------
// Exceptions
// ---------------------------------------------------------------------------

class Return : public std::exception
{
public:
    Return(std::shared_ptr<Value> value) : value_(std::move(value)) {}
    std::shared_ptr<Value> value() const { return value_; }

private:
    std::shared_ptr<Value> value_;
};

// ---------------------------------------------------------------------------
// AST node types
// ---------------------------------------------------------------------------

struct Symbol
{
    std::string name;
    Symbol(std::string n) : name(std::move(n)) {}
};

struct SExpr
{
    std::vector<std::shared_ptr<Value>> items;
    SExpr() = default;
    SExpr(std::vector<std::shared_ptr<Value>> i) : items(std::move(i)) {}
};

struct Lambda
{
    std::vector<std::string> params;
    std::shared_ptr<Value> body;
    std::shared_ptr<Env> closure;

    Lambda(std::vector<std::string> p, std::shared_ptr<Value> b,
           std::shared_ptr<Env> c)
        : params(std::move(p)), body(std::move(b)), closure(std::move(c))
    {
    }
};

struct Builtin
{
    std::function<std::shared_ptr<Value>(std::shared_ptr<Value>)> unary;
    std::function<std::shared_ptr<Value>(std::shared_ptr<Value>,
                                         std::shared_ptr<Value>)>
        binary;
    int arity;
    std::string name;
};

// ---------------------------------------------------------------------------
// Environment (lexical scoping via parent chain)
// ---------------------------------------------------------------------------

struct Env
{
    std::unordered_map<std::string, std::shared_ptr<Value>> bindings;
    std::shared_ptr<Env> parent;

    Env() = default;
    Env(std::shared_ptr<Env> p) : parent(std::move(p)) {}

    void define(const std::string &name, std::shared_ptr<Value> value);
    std::shared_ptr<Value> lookup(const std::string &name);
    void set(const std::string &name, std::shared_ptr<Value> value);
    std::shared_ptr<Env> extend();
};

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------

std::shared_ptr<Value> parse(const std::vector<Token> &tokens);

// ---------------------------------------------------------------------------
// Evaluator
// ---------------------------------------------------------------------------

std::shared_ptr<Value> eval_expr(std::shared_ptr<Value> expr,
                                 std::shared_ptr<Env> env);

// ---------------------------------------------------------------------------
// Global environment
// ---------------------------------------------------------------------------

std::shared_ptr<Env> make_global_env();