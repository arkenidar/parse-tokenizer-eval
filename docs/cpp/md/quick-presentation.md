# Quick Presentation

## What is it?

A C++17 port of a two‑file Python project that **tokenizes text into typed
values** and **executes prefix‑notation programs**.

- **~900 lines** total across headers + implementation
- **Zero dependencies** outside C++ standard library
- **Two layers**: tokenizer (`tokenize.cpp/hpp`) → evaluator (`eval.cpp/hpp`)
- **Standalone CLI** via `main.cpp`

## Tokenizer highlights

```cpp
#include "tokenize.hpp"
auto tokens = tokenize("first-word \"two words\" 42 true null");
// tokens = {
//   {WORD, "first-word"}, {TEXT, "two words"},
//   {INTEGER, "42"}, {BOOLEAN, "true"}, {NULL_TYPE, "null"}
// };
```

- Single compiled `std::regex` with ordered capture groups
- Whitespace-split with quoted text preservation
- JSON-like typing: WORD, TEXT, INTEGER, FLOAT, BOOLEAN, NULL_TYPE

## Evaluator highlights

```lisp
( def factorial ( n )
  ( if ( <= n 1 )
    1
    ( * n ( factorial ( - n 1 ) ) ) ) )
( factorial 5 )  → 120
```

- Prefix notation with spaced parens: `( + 1 2 )`
- Lexical scoping via parent-chain environments (`std::shared_ptr<Env>`)
- Special forms: `if`, `while`, `do`, `def`, `fun`, `set`, `return`
- Deferred evaluation (`if`/`while` only evaluate chosen paths)
- First-class functions with closures (`Lambda` captures `Env`)
- Exception-based `return` unwinding (`Return` exception)
- `shared_ptr`-based memory management with non‑owning parent env pointers

## Build and run

```bash
cd cpp
make
echo '( + 1 2 )' | ./eval
# → 3

echo '( def double ( x ) ( * x 2 ) ) ( double 21 )' | ./eval
# → 42
```

Or from a file:

```bash
./eval docs/examples/py/factorial.txt
# → 120
```
