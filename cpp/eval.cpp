#include "eval.hpp"

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

// ---------------------------------------------------------------------------
// Environment
// ---------------------------------------------------------------------------

void Env::define(const std::string &name, std::shared_ptr<Value> value)
{
    bindings[name] = std::move(value);
}

std::shared_ptr<Value> Env::lookup(const std::string &name)
{
    auto it = bindings.find(name);
    if (it != bindings.end())
        return it->second;
    if (parent)
        return parent->lookup(name);
    throw std::runtime_error("undefined: " + name);
}

void Env::set(const std::string &name, std::shared_ptr<Value> value)
{
    auto it = bindings.find(name);
    if (it != bindings.end())
    {
        bindings[name] = std::move(value);
        return;
    }
    if (parent)
    {
        try
        {
            parent->set(name, value);
            return;
        }
        catch (const std::runtime_error &)
        {
        }
    }
    bindings[name] = std::move(value);
}

std::shared_ptr<Env> Env::extend()
{
    return std::make_shared<Env>(std::shared_ptr<Env>(this,
                                                      [](Env *) {})); // non-owning
}

// ---------------------------------------------------------------------------
// Helpers: convert Token to Value atom
// ---------------------------------------------------------------------------

static std::shared_ptr<Value> token_to_atom(const Token &tok)
{
    switch (tok.kind)
    {
    case TokenType::WORD:
        return std::make_shared<Value>(
            std::make_shared<Symbol>(tok.text));
    case TokenType::TEXT:
        return std::make_shared<Value>(tok.text);
    case TokenType::INTEGER:
        return std::make_shared<Value>((int64_t)std::stoll(tok.text));
    case TokenType::FLOAT:
        return std::make_shared<Value>(std::stod(tok.text));
    case TokenType::BOOLEAN:
        return std::make_shared<Value>(tok.text == "true");
    case TokenType::NULL_TYPE:
        return std::make_shared<Value>(nullptr);
    }
    return std::make_shared<Value>(nullptr);
}

// ---------------------------------------------------------------------------
// Parser: flat token list → nested AST
// ---------------------------------------------------------------------------

std::shared_ptr<Value> parse(const std::vector<Token> &tokens)
{
    size_t pos = 0;

    std::function<std::shared_ptr<Value>()> parse_list;
    parse_list = [&]() -> std::shared_ptr<Value>
    {
        std::vector<std::shared_ptr<Value>> items;
        while (pos < tokens.size())
        {
            const Token &tok = tokens[pos];
            if (tok.kind == TokenType::WORD && tok.text == "(")
            {
                pos++; // consume '('
                items.push_back(parse_list());
            }
            else if (tok.kind == TokenType::WORD && tok.text == ")")
            {
                pos++; // consume ')'
                return std::make_shared<Value>(
                    std::make_shared<SExpr>(std::move(items)));
            }
            else
            {
                items.push_back(token_to_atom(tok));
                pos++;
            }
        }
        throw std::runtime_error("unexpected EOF inside list");
    };

    // Top-level: wrap in an implicit do-list if multiple top-level forms
    std::vector<std::shared_ptr<Value>> top;
    while (pos < tokens.size())
    {
        const Token &tok = tokens[pos];
        if (tok.kind == TokenType::WORD && tok.text == "(")
        {
            pos++; // consume '('
            top.push_back(parse_list());
        }
        else if (tok.kind == TokenType::WORD && tok.text == ")")
        {
            throw std::runtime_error("unexpected ')'");
        }
        else
        {
            top.push_back(token_to_atom(tok));
            pos++;
        }
    }

    if (top.size() == 1)
        return top[0];

    // Wrap multiple forms in (do ...)
    auto do_items = std::vector<std::shared_ptr<Value>>();
    do_items.push_back(
        std::make_shared<Value>(std::make_shared<Symbol>("do")));
    for (auto &t : top)
        do_items.push_back(t);
    return std::make_shared<Value>(
        std::make_shared<SExpr>(std::move(do_items)));
}

// ---------------------------------------------------------------------------
// Helpers: promote numbers for arithmetic ops
// ---------------------------------------------------------------------------

static double as_double(const Value &v)
{
    if (v.is_int())
        return (double)v.int_val;
    if (v.is_float())
        return v.float_val;
    throw std::runtime_error("expected number");
}

static std::shared_ptr<Value> num_result(double v)
{
    if (std::floor(v) == v && std::isfinite(v) &&
        v >= (double)INT64_MIN && v <= (double)INT64_MAX)
        return std::make_shared<Value>((int64_t)v);
    return std::make_shared<Value>(v);
}

// ---------------------------------------------------------------------------
// Built-in operators
// ---------------------------------------------------------------------------

static std::unordered_map<std::string, std::shared_ptr<Builtin>>
make_builtins()
{
    std::unordered_map<std::string, std::shared_ptr<Builtin>> b;

    auto make_bin = [&](const std::string &name,
                        std::function<double(double, double)> fn)
    {
        auto builtin = std::make_shared<Builtin>();
        builtin->arity = 2;
        builtin->name = name;
        builtin->binary = [fn](std::shared_ptr<Value> a,
                               std::shared_ptr<Value> b)
        {
            return num_result(fn(as_double(*a), as_double(*b)));
        };
        b[name] = builtin;
    };

    auto make_bin_bool = [&](const std::string &name,
                             std::function<bool(double, double)> fn)
    {
        auto builtin = std::make_shared<Builtin>();
        builtin->arity = 2;
        builtin->name = name;
        builtin->binary = [fn](std::shared_ptr<Value> a,
                               std::shared_ptr<Value> b)
        {
            return std::make_shared<Value>(fn(as_double(*a), as_double(*b)));
        };
        b[name] = builtin;
    };

    auto make_bool_bin = [&](const std::string &name,
                             std::function<bool(bool, bool)> fn)
    {
        auto builtin = std::make_shared<Builtin>();
        builtin->arity = 2;
        builtin->name = name;
        builtin->binary = [fn](std::shared_ptr<Value> a,
                               std::shared_ptr<Value> b)
        {
            return std::make_shared<Value>(
                fn(a->truthy(), b->truthy()));
        };
        b[name] = builtin;
    };

    auto make_una = [&](const std::string &name,
                        std::function<std::shared_ptr<Value>(
                            std::shared_ptr<Value>)>
                            fn)
    {
        auto builtin = std::make_shared<Builtin>();
        builtin->arity = 1;
        builtin->name = name;
        builtin->unary = fn;
        b[name] = builtin;
    };

    // --- arithmetic ---
    make_bin("+", [](double a, double b)
             { return a + b; });
    make_bin("-", [](double a, double b)
             { return a - b; });
    make_bin("*", [](double a, double b)
             { return a * b; });
    make_bin("/", [](double a, double b)
             { return a / b; });
    make_bin("mod", [](double a, double b)
             { return std::fmod(a, b); });

    // --- comparison ---
    make_bin_bool("<", [](double a, double b)
                  { return a < b; });
    make_bin_bool(">", [](double a, double b)
                  { return a > b; });
    make_bin_bool("<=", [](double a, double b)
                  { return a <= b; });
    make_bin_bool(">=", [](double a, double b)
                  { return a >= b; });
    make_bin_bool("=", [](double a, double b)
                  { return a == b; });
    make_bin_bool("!=", [](double a, double b)
                  { return a != b; });

    // --- logic ---
    make_bool_bin("and", [](bool a, bool b)
                  { return a && b; });
    make_bool_bin("or", [](bool a, bool b)
                  { return a || b; });

    // --- unary ---
    make_una("not", [](std::shared_ptr<Value> a)
             { return std::make_shared<Value>(!a->truthy()); });
    make_una("neg", [](std::shared_ptr<Value> a)
             { return num_result(-as_double(*a)); });

    // --- I/O ---
    make_una("print", [](std::shared_ptr<Value> a)
             {
        if (a->is_int()) std::cout << a->int_val;
        else if (a->is_float()) std::cout << a->float_val;
        else if (a->is_bool()) std::cout << (a->bool_val ? "true" : "false");
        else if (a->is_text()) std::cout << a->text_val;
        else if (a->is_nil()) std::cout << "null";
        else std::cout << "<value>";
        return a; });
    make_una("println", [](std::shared_ptr<Value> a)
             {
        if (a->is_int()) std::cout << a->int_val;
        else if (a->is_float()) std::cout << a->float_val;
        else if (a->is_bool()) std::cout << (a->bool_val ? "true" : "false");
        else if (a->is_text()) std::cout << a->text_val;
        else if (a->is_nil()) std::cout << "null";
        else std::cout << "<value>";
        std::cout << std::endl;
        return a; });

    // --- type checks ---
    make_una("is_null", [](std::shared_ptr<Value> a)
             { return std::make_shared<Value>(a->is_nil()); });
    make_una("is_bool", [](std::shared_ptr<Value> a)
             { return std::make_shared<Value>(a->is_bool()); });
    make_una("is_int", [](std::shared_ptr<Value> a)
             { return std::make_shared<Value>(a->is_int()); });
    make_una("is_float", [](std::shared_ptr<Value> a)
             { return std::make_shared<Value>(a->is_float()); });
    make_una("is_text", [](std::shared_ptr<Value> a)
             { return std::make_shared<Value>(a->is_text()); });
    make_una("is_word", [](std::shared_ptr<Value> a)
             { return std::make_shared<Value>(a->is_text()); });

    return b;
}

// ---------------------------------------------------------------------------
// Evaluator
// ---------------------------------------------------------------------------

static std::shared_ptr<Value>
apply_lambda(std::shared_ptr<Lambda> fn,
             const std::vector<std::shared_ptr<Value>> &args)
{
    if (args.size() != fn->params.size())
        throw std::runtime_error(
            "function expects " + std::to_string(fn->params.size()) +
            " args, got " + std::to_string(args.size()));

    auto call_env = fn->closure->extend();
    for (size_t i = 0; i < fn->params.size(); i++)
        call_env->define(fn->params[i], args[i]);

    try
    {
        return eval_expr(fn->body, call_env);
    }
    catch (const Return &r)
    {
        return r.value();
    }
}

std::shared_ptr<Value> eval_expr(std::shared_ptr<Value> expr,
                                 std::shared_ptr<Env> env)
{
    if (!expr)
        return std::make_shared<Value>(nullptr);

    // --- Literal values ---
    if (expr->is_int() || expr->is_float() || expr->is_bool() ||
        expr->is_text() || expr->is_nil())
        return expr;

    // --- Symbol lookup ---
    if (expr->is_symbol())
        return env->lookup(expr->symbol_val->name);

    // --- SExpr ---
    if (expr->is_sexpr())
    {
        auto &items = expr->sexpr_val->items;
        if (items.empty())
            return std::make_shared<Value>(nullptr);

        auto first = items[0];

        // Resolve the operator name
        if (first->is_symbol())
        {
            const std::string &opname = first->symbol_val->name;

            // --- Special forms ---

            if (opname == "if")
            {
                // (if cond then [else])
                auto cond = eval_expr(items[1], env);
                if (cond->truthy())
                    return eval_expr(items[2], env);
                else if (items.size() > 3)
                    return eval_expr(items[3], env);
                else
                    return std::make_shared<Value>(nullptr);
            }

            if (opname == "while")
            {
                // (while cond body)
                std::shared_ptr<Value> result =
                    std::make_shared<Value>(nullptr);
                while (eval_expr(items[1], env)->truthy())
                    result = eval_expr(items[2], env);
                return result;
            }

            if (opname == "do")
            {
                // (do expr1 expr2 ...)  — sequence, return last
                std::shared_ptr<Value> result =
                    std::make_shared<Value>(nullptr);
                for (size_t i = 1; i < items.size(); i++)
                    result = eval_expr(items[i], env);
                return result;
            }

            if (opname == "def")
            {
                // (def name (params) body)
                auto name_sym = items[1];
                if (!name_sym->is_symbol())
                    throw std::runtime_error(
                        "def: name must be a symbol");

                auto params_list = items[2];
                if (!params_list->is_sexpr())
                    throw std::runtime_error(
                        "def: params must be a list");

                std::vector<std::string> param_names;
                for (auto &p : params_list->sexpr_val->items)
                    if (p->is_symbol())
                        param_names.push_back(p->symbol_val->name);

                auto body =
                    items.size() > 3 ? items[3] : std::make_shared<Value>(nullptr);

                auto fn = std::make_shared<Lambda>(
                    param_names, body, env);

                env->define(name_sym->symbol_val->name,
                            std::make_shared<Value>(fn));
                return std::make_shared<Value>(fn);
            }

            if (opname == "fun")
            {
                // (fun (params) body)  — anonymous function
                auto params_list = items[1];
                if (!params_list->is_sexpr())
                    throw std::runtime_error(
                        "fun: params must be a list");

                std::vector<std::string> param_names;
                for (auto &p : params_list->sexpr_val->items)
                    if (p->is_symbol())
                        param_names.push_back(p->symbol_val->name);

                auto body =
                    items.size() > 2 ? items[2] : std::make_shared<Value>(nullptr);

                return std::make_shared<Value>(
                    std::make_shared<Lambda>(param_names, body, env));
            }

            if (opname == "set")
            {
                // (set name value)
                auto name_sym = items[1];
                if (!name_sym->is_symbol())
                    throw std::runtime_error(
                        "set: name must be a symbol");
                auto val = eval_expr(items[2], env);
                env->set(name_sym->symbol_val->name, val);
                return val;
            }

            if (opname == "return")
            {
                // (return [value])
                auto val = items.size() > 1
                               ? eval_expr(items[1], env)
                               : std::make_shared<Value>(nullptr);
                throw Return(val);
            }
        }

        // --- Function application ---
        auto op = eval_expr(first, env);
        std::vector<std::shared_ptr<Value>> args;
        for (size_t i = 1; i < items.size(); i++)
            args.push_back(eval_expr(items[i], env));

        // Built-in
        if (op->is_builtin())
        {
            auto &builtin = *op->builtin_val;
            if (builtin.arity == 1)
            {
                if (args.size() != 1)
                    throw std::runtime_error(
                        builtin.name + " expects 1 arg, got " +
                        std::to_string(args.size()));
                return builtin.unary(args[0]);
            }
            else if (builtin.arity == 2)
            {
                if (args.size() != 2)
                    throw std::runtime_error(
                        builtin.name + " expects 2 args, got " +
                        std::to_string(args.size()));
                return builtin.binary(args[0], args[1]);
            }
            else
            {
                throw std::runtime_error(
                    "unsupported builtin arity: " +
                    std::to_string(builtin.arity));
            }
        }

        // User-defined lambda
        if (op->is_lambda())
            return apply_lambda(op->lambda_val, args);

        throw std::runtime_error("not callable");
    }

    throw std::runtime_error("cannot evaluate expression");
}

// ---------------------------------------------------------------------------
// Global environment
// ---------------------------------------------------------------------------

std::shared_ptr<Env> make_global_env()
{
    auto env = std::make_shared<Env>();
    for (auto &pair : make_builtins())
        env->define(pair.first,
                    std::make_shared<Value>(pair.second));
    // Also bind true/false/null as literals
    env->define("true", std::make_shared<Value>(true));
    env->define("false", std::make_shared<Value>(false));
    env->define("null", std::make_shared<Value>(nullptr));
    return env;
}