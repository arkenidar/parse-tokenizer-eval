# Implementation

## Project Structure

```
cpp/
├── tokenize.hpp    # TokenType enum, Token struct, tokenize() declaration (24 lines)
├── tokenize.cpp    # regex tokenizer implementation (62 lines)
├── eval.hpp        # Value, Symbol, SExpr, Lambda, Builtin, Env, parser/evaluator declarations (217 lines)
├── eval.cpp        # parser + evaluator implementation (531 lines)
├── main.cpp        # CLI entry point (69 lines)
└── Makefile
```

## `tokenize.cpp` — Tokenizer

### TokenType Enum

```cpp
enum class TokenType
{
    WORD,       // unquoted identifier/symbol
    TEXT,       // "quoted string" (quotes stripped)
    INTEGER,    // integer literal
    FLOAT,      // float literal
    BOOLEAN,    // true / false
    NULL_TYPE   // null
};
```

### Token Struct

```cpp
struct Token
{
    TokenType kind;
    std::string text;   // raw matched text
};
```

Conversion to typed values happens in `token_to_atom()` inside `eval.cpp`.

### Compiled Regex

A single `static const std::regex` with **ordered capture groups**:

```cpp
static const std::regex tok_re(
    "\"([^\"]*)\"|(-?\\d+\\.\\d+)|(-?\\d+)|(true|false)|(null)|([^\\s]+)");
```

Groups are matched by index (C++ `std::regex` doesn't have named groups):

| Group | Pattern       | Captures                |
| ----- | ------------- | ----------------------- |
| 1     | `"([^"]*)"`   | Quoted text (no quotes) |
| 2     | `-?\d+\.\d+`  | Float literal           |
| 3     | `-?\d+`       | Integer literal         |
| 4     | `true\|false` | Boolean literal         |
| 5     | `null`        | Null literal            |
| 6     | `[^\s]+`      | Unquoted word           |

#### Ordering rationale

| Priority | Pattern         | Why here                                                                   |
| -------- | --------------- | -------------------------------------------------------------------------- |
| 1st      | `"([^"]*)"`     | Capture whitespace inside quotes                                           |
| 2nd      | float           | Must match before integer so `3.14` isn't partially matched as `3` + `.14` |
| 3rd      | integer         | Simple signed/unsigned digits                                              |
| 4th-5th  | true/false/null | Literal keywords                                                           |
| 6th      | `[^\s]+`        | Catch-all for any non-whitespace                                           |

### `tokenize()` Function

```cpp
std::vector<Token> tokenize(const std::string &text);
```

Iterates `std::sregex_iterator(text.begin(), text.end(), tok_re)`. For each
match:

- `m[1].matched` → group 1 is quoted text → `TokenType::TEXT`, `m[1].str()`
- `m[2].matched` → group 2 is float → `TokenType::FLOAT`, `m[2].str()`
- `m[3].matched` → group 3 is integer → `TokenType::INTEGER`, `m[3].str()`
- `m[4].matched` → group 4 is boolean → `TokenType::BOOLEAN`, `m[4].str()`
- `m[5].matched` → group 5 is null → `TokenType::NULL_TYPE`
- `m[6].matched` → group 6 is word → `TokenType::WORD`, `m[6].str()`

Conversion table:

| `m[N].matched` | C++ conversion in `token_to_atom()`  | `TokenType` |
| -------------- | ------------------------------------ | ----------- |
| `m[1]`         | `tok.text` (as-is string, no quotes) | `TEXT`      |
| `m[2]`         | `std::stod(tok.text)`                | `FLOAT`     |
| `m[3]`         | `std::stoll(tok.text)` → `int64_t`   | `INTEGER`   |
| `m[4]`         | `tok.text == "true"`                 | `BOOLEAN`   |
| `m[5]`         | `nullptr` → `Value(nullptr)`         | `NULL_TYPE` |
| `m[6]`         | `std::make_shared<Symbol>(tok.text)` | `WORD`      |

### Edge cases handled

- **Empty input** → empty vector (no regex matches)
- **Whitespace-only** → empty vector
- **Multi-line** → `\n` treated as whitespace separator; `\n` inside quotes
  preserved in `TEXT`
- **Number at word boundary** → float before int prevents partial match on
  `3.14`
- **Parens** → `(`, `)` are captured as WORD tokens (must be
  whitespace-separated)

## `eval.cpp` — Parser + Evaluator

### Module Include

```cpp
#include "eval.hpp"
```

Evaluator depends on token types and the `tokenize()` function declared in
`tokenize.hpp`.

### AST Nodes (4 structs + Value tagged union)

| Struct    | Fields                                                                                           | Heap-allocated via `shared_ptr` |
| --------- | ------------------------------------------------------------------------------------------------ | ------------------------------- |
| `Symbol`  | `std::string name`                                                                               | ✓                               |
| `SExpr`   | `std::vector<std::shared_ptr<Value>> items`                                                      | ✓                               |
| `Lambda`  | `std::vector<std::string> params`, `std::shared_ptr<Value> body`, `std::shared_ptr<Env> closure` | ✓                               |
| `Builtin` | `std::function<...> unary`, `std::function<...> binary`, `int arity`, `std::string name`         | ✓                               |

### Return Exception

```cpp
class Return : public std::exception
{
public:
    Return(std::shared_ptr<Value> value) : value_(std::move(value)) {}
    std::shared_ptr<Value> value() const { return value_; }
private:
    std::shared_ptr<Value> value_;
};
```

Used for `(return value)`. Caught in `apply_lambda()` via
`catch (const Return &r)` to convert back to a normal `shared_ptr<Value>`.

### Environment (`Env`)

**Parent‑chain** implementation of lexical scoping:

```cpp
struct Env
{
    std::unordered_map<std::string, std::shared_ptr<Value>> bindings;
    std::shared_ptr<Env> parent;

    void define(const std::string &name, std::shared_ptr<Value> value);
    std::shared_ptr<Value> lookup(const std::string &name);
    void set(const std::string &name, std::shared_ptr<Value> value);
    std::shared_ptr<Env> extend();
};
```

**`set()` semantics**: walks parent chain looking for existing binding; if
found, mutates it; if not found anywhere, creates a new binding in the current
scope. Uses `try/catch (const std::runtime_error &)` to detect "not found" in
parent chain rather than returning a boolean. This allows `(set x 10)` to work
as both first-assignment and mutation.

### Parser (`parse()`)

```cpp
std::shared_ptr<Value> parse(const std::vector<Token> &tokens);
```

**Algorithm** — recursive descent with a shared `size_t pos` index and
`std::function` lambda recursion:

```cpp
size_t pos = 0;
std::function<std::shared_ptr<Value>()> parse_list;
parse_list = [&]() -> std::shared_ptr<Value> {
    std::vector<std::shared_ptr<Value>> items;
    while (pos < tokens.size()) {
        if (tok.kind == WORD && tok.text == "(") {
            pos++; // consume '('
            items.push_back(parse_list());  // recurse
        } else if (tok.kind == WORD && tok.text == ")") {
            pos++; // consume ')'
            return SExpr(items);  // close
        } else {
            items.push_back(token_to_atom(tok));  // atom
            pos++;
        }
    }
    throw std::runtime_error("unexpected EOF inside list");
};
```

**Top-level handling**: if multiple top-level `(...)` forms exist, wrap them in
an implicit `(do ...)`:

```cpp
if (top.size() == 1) return top[0];
// wrap: SExpr([Symbol("do"), ...top])
```

The `std::function` captures `tokens` and `pos` by reference (`[&]`), allowing
the recursive lambda to refer to itself.

### Builtin Operators

`make_builtins()` returns an
`std::unordered_map<std::string, std::shared_ptr<Builtin>>`. Four helper lambdas
simplify registration:

```cpp
auto make_bin = [&](name, fn);       // binary numeric → numeric
auto make_bin_bool = [&](name, fn);  // binary numeric → bool
auto make_bool_bin = [&](name, fn);  // binary bool → bool
auto make_una = [&](name, fn);       // unary
```

Full list:

| Category    | Binary                          | Unary                                                            |
| ----------- | ------------------------------- | ---------------------------------------------------------------- |
| Arithmetic  | `+`, `-`, `*`, `/`, `mod`       | `neg`                                                            |
| Comparison  | `<`, `>`, `<=`, `>=`, `=`, `!=` |                                                                  |
| Logic       | `and`, `or`                     | `not`                                                            |
| I/O         |                                 | `print`, `println`                                               |
| Type checks |                                 | `is_null`, `is_bool`, `is_int`, `is_float`, `is_text`, `is_word` |

### Number Promotion

All arithmetic operators work on `double`. Two helpers handle conversion:

- **`as_double(const Value &v)`** — extracts `int_val` or `float_val` as
  `double`, throws if not numeric.
- **`num_result(double v)`** — if the result is a whole number within `int64_t`
  range, stores as `INT`; otherwise stores as `FLOAT`.

This means `( + 1 2 )` → `3` (int), but `( / 3 2 )` → `1.5` (float).

### Evaluator (`eval_expr()`)

**Dispatch logic**:

```
is INT|FLOAT|BOOL|TEXT|NIL → return expr (self-evaluating)
is SYMBOL → env->lookup(expr->symbol_val->name)
is SEXPR →
    if items empty → NIL
    first = items[0]
    if first is SYMBOL:
        dispatch on opname:
            "if"      → eval condition, evaluate chosen branch
            "while"   → loop: eval condition, eval body each iteration
            "do"      → sequence: eval each, return last
            "def"     → create Lambda, bind to name in env
            "fun"     → create Lambda (anonymous)
            "set"     → env->set(name, eval_expr(value))
            "return"  → throw Return(eval_expr(value))
    // fallthrough: function application
    operator = eval_expr(first, env)
    args = [eval_expr(a, env) for a in items[1:]]
    if BUILTIN → call with arity check (unary/binary)
    if LAMBDA → apply_lambda()
```

**`apply_lambda()`**:

```cpp
static std::shared_ptr<Value>
apply_lambda(std::shared_ptr<Lambda> fn,
             const std::vector<std::shared_ptr<Value>> &args)
{
    auto call_env = fn->closure->extend();
    for (size_t i = 0; i < fn->params.size(); i++)
        call_env->define(fn->params[i], args[i]);

    try {
        return eval_expr(fn->body, call_env);
    } catch (const Return &r) {
        return r.value();  // unwrap return
    }
}
```

### Global Environment (`make_global_env()`)

Creates the top-level `Env`, populates it with all builtins from
`make_builtins()`, and binds `true` → `true`, `false` → `false`, `null` →
`nullptr`.

### CLI Entry Point (`main.cpp`)

```
source = open(argv[1]) if argc > 1 else std::cin
text = read all (file or stdin)
tokens = tokenize(text)
if tokens.empty() → return 0
ast = parse(tokens)
env = make_global_env()
result = eval_expr(ast, env)
print result (type-aware formatting)
```

The output printer handles each `Value::Tag` with appropriate formatting
(`int_val`, `float_val`, `bool_val` → `"true"/"false"`, `text_val`, `nil` →
`"null"`, `LAMBDA` → `"<lambda>"`, `BUILTIN` → `"<builtin>"`).

## Design Constraints

1. **No tokenizer modification for eval** — `tokenize.cpp` is a standalone,
   general-purpose tokenizer. The evaluator imposes syntax conventions (spaced
   parens, `is_null` instead of `null?`) rather than patching the tokenizer.
2. **No external dependencies** — only C++17 standard library (`<regex>`,
   `<memory>`, `<unordered_map>`, `<functional>`, `<cmath>`, `<fstream>`,
   `<sstream>`).
3. **Single-pass tokenization** — `std::sregex_iterator` greedy matching with
   ordered alternation.
4. **Exception-based return** — avoids threading a `return_value` through every
   `eval_expr` call.
5. **`shared_ptr` for memory** — all heap objects use reference counting; no
   manual `new`/`delete`, no GC required. Non‑owning aliasing `shared_ptr` for
   the `Env` parent chain prevents cycles.
