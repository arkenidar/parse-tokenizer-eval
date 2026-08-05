# Usages

## CLI

### From a file

```bash
python3 py/eval.py program.txt
```

### From stdin

```bash
echo '( + 1 2 )' | python3 py/eval.py
```

### Tokenizer only

```bash
# print token list from a file
python3 py/parse.py text1.txt

# from stdin
echo 'first-word "second word" third_word' | python3 py/parse.py
```

## Programmatic API

### Importing the tokenizer

```python
from parse import tokenize, TokenType

tokens = tokenize('hello 42 "two words" true null')
# [
#   (TokenType.WORD, 'hello'),
#   (TokenType.INTEGER, 42),
#   (TokenType.TEXT, 'two words'),
#   (TokenType.BOOLEAN, True),
#   (TokenType.NULL, None),
# ]
```

### Importing the evaluator

```python
from parse import tokenize
from eval import parse, make_global_env, eval_expr

# Each call needs a fresh env, or reuse one for stateful scripts
env = make_global_env()

tokens = tokenize('( + 1 2 )')
ast = parse(tokens)
result = eval_expr(ast, env)
print(result)  # 3
```

### Reusable environment for multi-expression scripts

```python
env = make_global_env()

for src in ['( set x 10 )', '( set y 20 )', '( + x y )']:
    tokens = tokenize(src)
    ast = parse(tokens)
    result = eval_expr(ast, env)

print(result)  # 30
```

## Syntax Reference

### Convention: spaces around parentheses

```
✓ ( + 1 2 )
✓ ( if ( < 3 5 ) 1 0 )
✗ (+ 1 2)      ← paren fused with symbol, will fail
```

### Literals

| Type    | Examples                          |
| ------- | --------------------------------- |
| Integer | `42`, `-7`, `0`                   |
| Float   | `3.14`, `-0.5`, `1.0`             |
| Boolean | `true`, `false`                   |
| Null    | `null`                            |
| Text    | `"hello world"`, `"line1\nline2"` |

### Special Forms

#### `( if cond then [else] )`

Conditional with deferred evaluation. Only the chosen branch is evaluated.

```lisp
( if true 42 99 )            → 42
( if ( > 5 3 ) "yes" "no" )  → "yes"
( if false 1 )                → None
```

#### `( while cond body )`

Loop while condition is truthy. Returns the last body value.

```lisp
( do
  ( set i 3 )
  ( set sum 0 )
  ( while ( > i 0 )
    ( do
      ( set sum ( + sum i ) )
      ( set i ( - i 1 ) ) ) )
  sum )
→ 6
```

#### `( do expr1 expr2 ... )`

Sequential evaluation. Returns the result of the last expression.

```lisp
( do
  ( set x 5 )
  ( set y 10 )
  ( + x y ) )
→ 15
```

#### `( def name ( params ) body )`

Define a named function in the current environment. Captures lexical closure.

```lisp
( def double ( x ) ( * x 2 ) )
( double 5 )   → 10
```

#### `( fun ( params ) body )`

Anonymous function (lambda). Captures lexical closure.

```lisp
( ( fun ( x ) ( * x x ) ) 7 )   → 49
```

#### `( set name value )`

Assign or mutate a variable in scope.

```lisp
( set x 42 )
( set x ( + x 8 ) )
x   → 50
```

#### `( return [value] )`

Early return from a function. Returns `None` if no value given.

```lisp
( def safe-div ( a b )
  ( if ( = b 0 )
    ( return null )
    ( / a b ) ) )
( safe-div 10 2 )  → 5.0
( safe-div 10 0 )  → None
```

### Built-in Operators

#### Arithmetic (binary, except `neg`)

| Operator | Example        | Result |
| -------- | -------------- | ------ |
| `+`      | `( + 1 2 3 )`  | `6`    |
| `-`      | `( - 10 3 )`   | `7`    |
| `*`      | `( * 4 5 )`    | `20`   |
| `/`      | `( / 10 2 )`   | `5.0`  |
| `mod`    | `( mod 10 3 )` | `1`    |
| `neg`    | `( neg 5 )`    | `-5`   |

#### Comparison (binary)

| Operator | Example      | Result |
| -------- | ------------ | ------ |
| `<`      | `( < 1 2 )`  | `True` |
| `>`      | `( > 5 3 )`  | `True` |
| `<=`     | `( <= 3 3 )` | `True` |
| `>=`     | `( >= 5 5 )` | `True` |
| `=`      | `( = 3 3 )`  | `True` |
| `!=`     | `( != 1 2 )` | `True` |

#### Logic

| Operator | Example              | Result  |
| -------- | -------------------- | ------- |
| `and`    | `( and true false )` | `False` |
| `or`     | `( or true false )`  | `True`  |
| `not`    | `( not true )`       | `False` |

#### I/O (unary)

| Operator  | Effect                    | Returns |
| --------- | ------------------------- | ------- |
| `print`   | Prints value (no newline) | value   |
| `println` | Prints value with newline | value   |

#### Type Checks (unary)

| Operator   | Example             | Result |
| ---------- | ------------------- | ------ |
| `is_null`  | `( is_null null )`  | `True` |
| `is_bool`  | `( is_bool true )`  | `True` |
| `is_int`   | `( is_int 42 )`     | `True` |
| `is_float` | `( is_float 3.14 )` | `True` |
| `is_text`  | `( is_text "hi" )`  | `True` |
| `is_word`  | `( is_word hello )` | `True` |

## Complete Examples

### Recursive factorial

```lisp
( do
  ( def factorial ( n )
    ( if ( <= n 1 )
      1
      ( * n ( factorial ( - n 1 ) ) ) ) )
  ( factorial 5 ) )
→ 120
```

### Higher-order function

```lisp
( do
  ( def double ( x ) ( * x 2 ) )
  ( def apply-twice ( f x ) ( f ( f x ) ) )
  ( apply-twice double 3 ) )
→ 12
```

### Iteration with while

```lisp
( do
  ( set n 10 )
  ( set result 0 )
  ( while ( > n 0 )
    ( do
      ( set result ( + result n ) )
      ( set n ( - n 1 ) ) ) )
  result )
→ 55
```
