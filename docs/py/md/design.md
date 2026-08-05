# Design

## Two-Layer Architecture

```
Input Text
    │
    ▼
┌─────────────┐
│  parse.py   │  Tokenizer — regex‑based, single‑pass
│  tokenize() │  produces flat list of (TokenType, value) pairs
└──────┬──────┘
       │  List[(TokenType, Any)]
       ▼
┌─────────────┐
│  eval.py    │  Parser + Evaluator
│  parse()    │  recursive descent → SExpr AST
│  eval_expr()│  tree‑walking interpreter
└──────┬──────┘
       │
       ▼
    Result
```

The tokenizer is intentionally **agnostic** about syntax — it only classifies
atoms.\
The evaluator imposes **prefix‑notation / S‑expression** structure on top.

## Tokenizer (`parse.py`)

### Design Decisions

- **Single compiled regex** with ordered named groups. Python's `re` module
  tries each alternative left‑to‑right at every match position.
- **Quoted text** (`"[^"]*"`) comes first so inner whitespace is captured before
  the catch‑all word pattern.
- **Float before integer** so `3.14` is not partially matched as `3` + `.14`.
- **No `\b` / lookahead guards** on `true`/`false`/`null` — simplicity over
  "does the token look like `trueStuff`". The evaluator layer uses plain‑WORD
  names without `?` suffix to avoid ambiguity.
- **No `(` / `)` extraction** — the parser requires spaces around parens
  (`( + 1 2 )`). This keeps the tokenizer regex minimal.

### Output

A flat Python `list` of `(TokenType, value)`:

| TokenType | Python `value`    | Example token text |
| --------- | ----------------- | ------------------ |
| `WORD`    | `str`             | `hello`, `(`, `+`  |
| `TEXT`    | `str` (no quotes) | `"two words"`      |
| `INTEGER` | `int`             | `42`, `-7`         |
| `FLOAT`   | `float`           | `3.14`, `-0.5`     |
| `BOOLEAN` | `bool`            | `true`, `false`    |
| `NULL`    | `None`            | `null`             |

## Parser (`parse()`)

### Recursive Descent

A single shared iterator over the token list. `(` pushes into a nested
`_parse_list()`, `)` pops and returns an `SExpr` object. All non‑paren tokens
become AST atoms (literal values or `Symbol` instances).

Multiple top‑level expressions are implicitly wrapped in a `(do ...)` block so
the evaluator sees a single root node.

### AST Node Types

| Class     | Represents                                    |
| --------- | --------------------------------------------- |
| `Symbol`  | Named reference (operator, variable, keyword) |
| `SExpr`   | Parenthesised list                            |
| `Lambda`  | User‑defined function + closure environment   |
| `Builtin` | Binary or unary callable                      |

### Why spaces around parens?

Tokens like `(+` or `))` cause the parser to see fused symbols. Requiring `( +`
and `) )` lets the original `[^\s]+` word pattern split parens naturally — no
preprocessor or token‑patching layer in `eval.py`.

## Evaluator (`eval_expr()`)

### Dispatch Strategy

```
eval_expr(expr, env):
  literal (int|float|bool|str|None) → value
  Symbol                            → env.lookup(name)
  SExpr:
    first is Symbol → special form dispatch
    otherwise       → function application
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
| `return` | Early exit via Python exception unwind            |

### Function Application

Non‑special‑form `SExpr` heads are evaluated as a callable:

1. Evaluate the operator (first item)
2. Evaluate all arguments
3. If `Builtin` → call with checked arity
4. If `Lambda` → `_apply_lambda()`: extend closure env, bind params, evaluate
   body, trap `Return`

## Environment (`Env`)

**Lexical scoping** via parent‑chain:

```
Global Env
  ├── +, -, *, ...  (builtins)
  ├── true, false, null
  │
  └── (def f ...)  →  Env child for f's call
        ├── param1 = arg1
        └── param2 = arg2
```

- `define()` — add binding to **this** scope (shadows parent)
- `lookup()` — walk up parent chain, raise if not found
- `set()` — mutate existing binding in the nearest scope that has it, or define
  locally
- `extend()` — create a new child Env

## Return Mechanism

`(return value)` raises a `Return(value)` Python exception. `_apply_lambda()`
catches it to convert the exception back to a normal return value. Nested `do`
blocks unwind cleanly through the exception stack.
