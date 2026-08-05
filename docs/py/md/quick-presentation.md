# Quick Presentation

## What is it?

A two-file Python project that **tokenizes text into typed values** and
**executes prefix-notation programs**.

- **440 lines** total (62 + 378)
- **Zero dependencies** outside Python stdlib
- **Two layers**: tokenizer (`parse.py`) → evaluator (`eval.py`)

## Tokenizer highlights

```python
from parse import tokenize
tokenize('first-word "two words" 42 true null')
# → [(WORD, 'first-word'), (TEXT, 'two words'),
#    (INTEGER, 42), (BOOLEAN, True), (NULL, None)]
```

- Single compiled regex with ordered named groups
- Whitespace-split with quoted text preservation
- JSON-like typing: int, float, bool, None, str

## Evaluator highlights

```lisp
( def factorial ( n )
  ( if ( <= n 1 )
    1
    ( * n ( factorial ( - n 1 ) ) ) ) )
( factorial 5 )  → 120
```

- Prefix notation with spaced parens: `( + 1 2 )`
- Lexical scoping via parent-chain environments
- Special forms: `if`, `while`, `do`, `def`, `fun`, `set`, `return`
- Deferred evaluation (`if`/`while` only evaluate chosen paths)
- First-class functions with closures
- Exception-based `return` unwinding

## Run it

```bash
echo '( + 1 2 )' | python3 py/eval.py
# → 3
```
