# Implementation

## Project Structure

```
py/
├── parse.py    # tokenizer (62 lines)
└── eval.py     # parser + evaluator (378 lines)
```

## `parse.py` — Tokenizer

### TokenType Enum

```python
class TokenType(Enum):
    WORD = auto()      # unquoted identifier/symbol
    TEXT = auto()      # "quoted string" (quotes stripped)
    INTEGER = auto()   # integer literal
    FLOAT = auto()     # float literal
    BOOLEAN = auto()   # true / false
    NULL = auto()      # null
```

### Compiled Regex

A single `re.compile()` with **ordered named groups**:

```python
_TOKEN_RE = re.compile(
    r'"(?P<text>[^"]*)"'           # 1. quoted text
    r'|(?P<float>-?\d+\.\d+)'      # 2. float (before int!)
    r'|(?P<integer>-?\d+)'         # 3. integer
    r'|(?P<boolean>true|false)'    # 4. boolean literal
    r'|(?P<null>null)'             # 5. null literal
    r'|(?P<word>[^\s]+)'           # 6. unquoted word (catch-all)
)
```

#### Ordering rationale

| Priority | Pattern         | Why here                                                                   |
| -------- | --------------- | -------------------------------------------------------------------------- |
| 1st      | `"[^"]*"`       | Capture whitespace inside quotes                                           |
| 2nd      | float           | Must match before integer so `3.14` isn't partially matched as `3` + `.14` |
| 3rd      | integer         | Simple signed/unsigned digits                                              |
| 4th-5th  | true/false/null | Literal keywords                                                           |
| 6th      | `[^\s]+`        | Catch-all for any non-whitespace                                           |

### `tokenize()` Function

```python
def tokenize(text: str) -> List[Tuple[TokenType, Any]]:
```

Iterates `_TOKEN_RE.finditer(text)`. For each match:

- `m.lastgroup` → name of the regex group that matched
- `m.group(0)` → raw matched text
- Converts to typed value based on group name

Conversion table:

| `m.lastgroup` | Python conversion                     | TokenType |
| ------------- | ------------------------------------- | --------- |
| `text`        | `m.group("text")` — raw inner content | `TEXT`    |
| `float`       | `float(value)`                        | `FLOAT`   |
| `integer`     | `int(value)`                          | `INTEGER` |
| `boolean`     | `value == "true"`                     | `BOOLEAN` |
| `null`        | `None`                                | `NULL`    |
| `word`        | `value` (as-is string)                | `WORD`    |

### Edge cases handled

- **Empty input** → `[]`
- **Whitespace-only** → `[]` (no matches from regex)
- **Multi-line** → `\n` treated as whitespace separator; `\n` inside quotes
  preserved in `TEXT`
- **Number at word boundary** → float before int prevents partial match on
  `3.14`
- **Parens** → `(`, `)` are captured as WORD tokens (must be
  whitespace-separated)

## `eval.py` — Parser + Evaluator

### Module Imports

```python
from parse import TokenType, tokenize
```

Evaluator depends only on the public API of `parse.py`.

### AST Nodes (4 classes)

| Class     | Fields                                           | `__slots__` for memory efficiency |
| --------- | ------------------------------------------------ | --------------------------------- |
| `Symbol`  | `name: str`                                      | ✓                                 |
| `SExpr`   | `items: List[Any]`                               | ✓                                 |
| `Lambda`  | `params: List[str]`, `body: Any`, `closure: Env` | ✓                                 |
| `Builtin` | `func: Callable`, `arity: int`, `name: str`      | ✓                                 |

### Return Exception

```python
class Return(Exception):
    def __init__(self, value):
        self.value = value
```

Used for `(return value)`. Caught in `_apply_lambda()` to convert back to a
normal return value.

### Environment (`Env`)

**Parent-chain** implementation of lexical scoping:

```python
class Env:
    _bindings: Dict[str, Any]   # current scope
    _parent: Optional[Env]      # enclosing scope (None = global)

    def define(self, name, value):  # add to *this* scope
    def lookup(self, name):         # walk chain upward
    def set(self, name, value):     # mutate nearest binding
    def extend(self):               # new child: Env(parent=self)
```

**`set()` semantics**: walks parent chain looking for existing binding; if
found, mutates it; if not found anywhere, creates a new binding in the current
scope. This allows `(set x 10)` to work as both first-assignment and mutation.

### Parser (`parse()`)

```python
def parse(tokens: List[Tuple[TokenType, Any]]) -> Any:
```

**Algorithm** — recursive descent with a shared iterator:

```python
it = iter(enumerate(tokens))

def _parse_list():
    items = []
    for _idx, tok in it:
        if tok is WORD "(" → items.append(_parse_list())   # recurse
        elif tok is WORD ")" → return SExpr(items)          # close
        else → items.append(_token_to_atom(tok))            # atom
    raise SyntaxError("unexpected EOF inside list")
```

**Top-level handling**: if multiple top-level forms exist, wrap them in an
implicit `(do ...)`:

```python
if len(top) == 1: return top[0]
return SExpr([Symbol("do")] + top)
```

**`_token_to_atom()`**: WORD → `Symbol(value)`, anything else → literal (int,
float, bool, None).

### Builtin Operators

`_make_builtins()` returns a `Dict[str, Builtin]`. Two helper closures simplify
registration:

```python
def _bin(name, fn):  b[name] = Builtin(fn, arity=2, name=name)
def _una(name, fn):  b[name] = Builtin(fn, arity=1, name=name)
```

Full list:

| Category    | Binary                          | Unary                                                            |
| ----------- | ------------------------------- | ---------------------------------------------------------------- |
| Arithmetic  | `+`, `-`, `*`, `/`, `mod`       | `neg`                                                            |
| Comparison  | `<`, `>`, `<=`, `>=`, `=`, `!=` |                                                                  |
| Logic       | `and`, `or`                     | `not`                                                            |
| I/O         |                                 | `print`, `println`                                               |
| Type checks |                                 | `is_null`, `is_bool`, `is_int`, `is_float`, `is_text`, `is_word` |

### Evaluator (`eval_expr()`)

**Dispatch logic**:

```
isinstance(expr, (int, float, bool, str)) or expr is None → return expr
isinstance(expr, Symbol) → env.lookup(expr.name)
isinstance(expr, SExpr)  →
    if items empty → None
    first = items[0]
    if first is Symbol:
        dispatch on opname:
            "if"      → eval condition, evaluate chosen branch
            "while"   → loop: eval condition, eval body each iteration
            "do"      → sequence: eval each, return last
            "def"     → create Lambda, bind to name in env
            "fun"     → create Lambda (anonymous)
            "set"     → env.set(name, eval_expr(value))
            "return"  → raise Return(eval_expr(value))
    // fallthrough: function application
    operator = eval_expr(first, env)
    args = [eval_expr(a, env) for a in items[1:]]
    if Builtin → call with arity check
    if Lambda → _apply_lambda()
```

**`_apply_lambda()`**:

```python
def _apply_lambda(fn, args):
    call_env = fn.closure.extend()       # child of closure scope
    for name, val in zip(fn.params, args):
        call_env.define(name, val)        # bind params
    try:
        return eval_expr(fn.body, call_env)
    except Return as r:
        return r.value                    # unwrap return
```

### Global Environment (`make_global_env()`)

Creates the top-level `Env`, populates it with all builtins from
`_make_builtins()`, and binds `true` → `True`, `false` → `False`, `null` →
`None`.

### CLI Entry Point (`_main()`)

```
source = open(argv[1]) if argv else sys.stdin
text = source.read()
tokens = tokenize(text)
ast = parse(tokens)
env = make_global_env()
result = eval_expr(ast, env)
print(result)
```

## Design Constraints

1. **No tokenizer modification for eval** — `parse.py` is a standalone,
   general-purpose tokenizer. The evaluator imposes syntax conventions (spaced
   parens, `is_null` instead of `null?`) rather than patching the tokenizer.
2. **No external dependencies** — only Python 3 stdlib (`re`, `enum`, `typing`).
3. **Single-pass tokenization** — `re.finditer()` greedy matching with ordered
   alternation.
4. **Exception-based return** — avoids threading a `return_value` through every
   `eval_expr` call.
