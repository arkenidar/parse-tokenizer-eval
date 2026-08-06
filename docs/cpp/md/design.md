# Design

## Two-Layer Architecture

```
Input Text
    │
    ▼
┌───────────────┐
│ tokenize.cpp  │  Tokenizer — regex‑based, single‑pass
│ tokenize()    │  produces flat list of Token structs
└───────┬───────┘
        │  std::vector<Token>
        ▼
┌───────────────┐
│ eval.cpp      │  Parser + Evaluator
│ parse()       │  recursive descent → Value AST (shared_ptr)
│ eval_expr()   │  tree‑walking interpreter
└───────┬───────┘
        │
        ▼
     Result
```

The tokenizer is intentionally **agnostic** about syntax — it only classifies
atoms. The evaluator imposes **prefix‑notation / S‑expression** structure on
top.

## Tokenizer (`tokenize.cpp`)

### Design Decisions

- **Single compiled `std::regex`** with ordered capture groups.
  `std::sregex_iterator` visits matches left‑to‑right at every position.
- **Quoted text** (`"([^"]*)"`) comes first so inner whitespace is captured
  before the catch‑all word pattern.
- **Float before integer** so `3.14` is not partially matched as `3` + `.14`.
- **No `true`/`false`/`null` with lookahead guards** — simplicity over "does the
  token look like `trueStuff`". The evaluator layer uses plain‑WORD names to
  avoid ambiguity.
- **No `(` / `)` extraction** — the parser requires spaces around parens
  (`( + 1 2 )`). This keeps the tokenizer regex minimal.
- **Raw text stored in `Token::text`** — conversion to typed values happens
  later in `token_to_atom()`.

### Output

A flat `std::vector<Token>`:

| `TokenType` | C++ type in `Token::text` | Example token text |
| ----------- | ------------------------- | ------------------ |
| `WORD`      | `std::string`             | `hello`, `(`, `+`  |
| `TEXT`      | `std::string` (no quotes) | `"two words"`      |
| `INTEGER`   | `std::string` (digits)    | `42`, `-7`         |
| `FLOAT`     | `std::string` (digits)    | `3.14`, `-0.5`     |
| `BOOLEAN`   | `std::string`             | `true`, `false`    |
| `NULL_TYPE` | `std::string`             | `null`             |

## Parser (`parse()`)

### Recursive Descent

A single shared `size_t pos` index over the token vector. `(` increments `pos`
and calls `parse_list()` recursively via a `std::function` lambda, `)` returns a
`Value(SExpr(...))`. All non‑paren tokens become AST atoms via
`token_to_atom()`.

Multiple top‑level expressions are implicitly wrapped in a `(do ...)` block so
the evaluator sees a single root node.

### AST Node Types

| Struct    | Represented by `Value` tag             |
| --------- | -------------------------------------- |
| `Symbol`  | `Tag::SYMBOL` — `shared_ptr<Symbol>`   |
| `SExpr`   | `Tag::SEXPR` — `shared_ptr<SExpr>`     |
| `Lambda`  | `Tag::LAMBDA` — `shared_ptr<Lambda>`   |
| `Builtin` | `Tag::BUILTIN` — `shared_ptr<Builtin>` |

### Value — Tagged Union

The C++ implementation uses a manual tagged union inside `Value` rather than
`std::variant`:

| Tag       | Active field  | C++ type                   |
| --------- | ------------- | -------------------------- |
| `INT`     | `int_val`     | `int64_t`                  |
| `FLOAT`   | `float_val`   | `double`                   |
| `BOOL`    | `bool_val`    | `bool`                     |
| `TEXT`    | `text_val`    | `std::string`              |
| `NIL`     | —             | —                          |
| `SYMBOL`  | `symbol_val`  | `std::shared_ptr<Symbol>`  |
| `SEXPR`   | `sexpr_val`   | `std::shared_ptr<SExpr>`   |
| `LAMBDA`  | `lambda_val`  | `std::shared_ptr<Lambda>`  |
| `BUILTIN` | `builtin_val` | `std::shared_ptr<Builtin>` |

### Why spaces around parens?

Tokens like `(+` or `))` cause the parser to see fused symbols. Requiring `( +`
and `) )` lets the `[^\s]+` word pattern split parens naturally — no
preprocessor or token‑patching layer in `eval.cpp`.

## Evaluator (`eval_expr()`)

### Dispatch Strategy

```
eval_expr(expr, env):
  INT|FLOAT|BOOL|TEXT|NIL     → expr (self-evaluating literal)
  SYMBOL                      → env->lookup(name)
  SEXPR:
    items empty               → NIL
    first is SYMBOL:
      "if"      → deferred conditional
      "while"   → deferred loop
      "do"      → sequential evaluation
      "def"     → capture closure, store Lambda
      "fun"     → anonymous Lambda
      "set"     → env->set(name, value)
      "return"  → throw Return(value)
    // fallthrough: function application
    operator = eval_expr(first, env)
    args = eval_expr(each rest, env)
    BUILTIN → call with arity check
    LAMBDA  → apply_lambda()
```

### Special Forms

Reserved by **symbol name** at evaluation time (not by syntax):

| Form     | Behaviour                                         |
| -------- | ------------------------------------------------- |
| `if`     | Deferred branch — only chosen branch is evaluated |
| `while`  | Deferred loop — body evaluated each iteration     |
| `do`     | Sequential evaluation, returns last result        |
| `def`    | Captures closure `env`, stores `Lambda`           |
| `fun`    | Anonymous `Lambda`                                |
| `set`    | Mutate or create variable                         |
| `return` | Early exit via C++ exception unwind               |

### Function Application

Non‑special‑form `SExpr` heads are evaluated as a callable:

1. Evaluate the operator (first item)
2. Evaluate all arguments
3. If `BUILTIN` → call with checked arity (`unary` or `binary` function)
4. If `LAMBDA` → `apply_lambda()`: extend closure env, bind params, evaluate
   body, catch `Return`

## Environment (`Env`)

**Lexical scoping** via parent‑chain using `std::shared_ptr<Env>`:

```
Global Env
  ├── +, -, *, ...  (builtins as Builtin shared_ptrs)
  ├── true, false, null
  │
  └── (def f ...)  →  Env child for f's call
        ├── param1 = arg1
        └── param2 = arg2
```

- `define()` — add binding to **this** scope (shadows parent)
- `lookup()` — walk up parent chain via `shared_ptr`, raise `std::runtime_error`
  if not found
- `set()` — mutate existing binding in the nearest scope that has it (walking
  parent chain, catching `runtime_error` on misses), or define locally
- `extend()` — create new child `Env` with non‑owning parent pointer (aliasing
  constructor: `shared_ptr(this, [](Env*){}))`)

### Non‑owning parent pointer

The `Env::extend()` method uses C++ `std::shared_ptr` aliasing constructor:

```cpp
std::make_shared<Env>(std::shared_ptr<Env>(this, [](Env *) {}));
```

This creates a new `Env` whose `parent` field holds a non‑owning reference to
`this`. The empty deleter `[](Env*){}` prevents double‑delete. This pattern
avoids reference cycles while keeping the `shared_ptr` API consistent.

## Return Mechanism

`(return value)` throws a `Return(value)` C++ exception. `apply_lambda()`
catches it via `catch (const Return &r)` to convert the exception back to a
normal `shared_ptr<Value>`. Nested `do` blocks unwind cleanly through the
exception stack.

## Memory Management

All heap‑allocated AST nodes and runtime values use `std::shared_ptr<T>`. This
provides automatic, reference‑counted lifetime management without a garbage
collector or manual `delete`. The `Env` parent chain is the only place where
cycles could occur — solved by the non‑owning aliasing `shared_ptr`.
