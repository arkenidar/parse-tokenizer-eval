# Usages

## CLI

### Build

```bash
cd cpp
make
```

### From a file

```bash
./eval program.txt
```

### From stdin

```bash
echo '( + 1 2 )' | ./eval
```

### Tokenizer only

The tokenizer doesn't have a standalone CLI in C++ (unlike Python's `parse.py`).
To inspect tokens, you can write a small test program:

```cpp
#include "tokenize.hpp"
#include <iostream>

int main() {
    auto tokens = tokenize("hello 42 \"two words\" true null");
    for (auto &t : tokens) {
        std::cout << (int)t.kind << ": " << t.text << std::endl;
    }
}
```

Or use the Python tokenizer from the sibling project for exploration:

```bash
echo 'first-word "second word" third_word' | python3 py/parse.py
```

## Programmatic API

### Including the tokenizer

```cpp
#include "tokenize.hpp"

auto tokens = tokenize("hello 42 \"two words\" true null");
// tokens = {
//   {TokenType::WORD, "hello"},
//   {TokenType::INTEGER, "42"},
//   {TokenType::TEXT, "two words"},
//   {TokenType::BOOLEAN, "true"},
//   {TokenType::NULL_TYPE, "null"},
// };
```

### Including the evaluator

```cpp
#include "eval.hpp"

// Each pipeline needs a fresh env, or reuse one for stateful scripts
auto env = make_global_env();

auto tokens = tokenize("( + 1 2 )");
auto ast = parse(tokens);
auto result = eval_expr(ast, env);
// result->int_val == 3
```

### Reusable environment for multi-expression scripts

```cpp
#include "eval.hpp"

auto env = make_global_env();

for (auto src : {"( set x 10 )", "( set y 20 )", "( + x y )"}) {
    auto tokens = tokenize(src);
    auto ast = parse(tokens);
    auto result = eval_expr(ast, env);
}
// last result->int_val == 30
```

### Compiling and linking

```bash
g++ -std=c++17 -c tokenize.cpp -o tokenize.o
g++ -std=c++17 -c eval.cpp -o eval.o
g++ -std=c++17 your_program.cpp tokenize.o eval.o -o your_program
```

Or add your source files to the Makefile.

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

Early return from a function. Returns `null` if no value given.

```lisp
( def safe-div ( a b )
  ( if ( = b 0 )
    ( return null )
    ( / a b ) ) )
( safe-div 10 2 )  → 5
( safe-div 10 0 )  → null
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
| `<`      | `( < 1 2 )`  | `true` |
| `>`      | `( > 5 3 )`  | `true` |
| `<=`     | `( <= 3 3 )` | `true` |
| `>=`     | `( >= 5 5 )` | `true` |
| `=`      | `( = 3 3 )`  | `true` |
| `!=`     | `( != 1 2 )` | `true` |

#### Logic

| Operator | Example              | Result  |
| -------- | -------------------- | ------- |
| `and`    | `( and true false )` | `false` |
| `or`     | `( or true false )`  | `true`  |
| `not`    | `( not true )`       | `false` |

#### I/O (unary)

| Operator  | Effect                    | Returns |
| --------- | ------------------------- | ------- |
| `print`   | Prints value (no newline) | value   |
| `println` | Prints value with newline | value   |

#### Type Checks (unary)

| Operator   | Example             | Result |
| ---------- | ------------------- | ------ |
| `is_null`  | `( is_null null )`  | `true` |
| `is_bool`  | `( is_bool true )`  | `true` |
| `is_int`   | `( is_int 42 )`     | `true` |
| `is_float` | `( is_float 3.14 )` | `true` |
| `is_text`  | `( is_text "hi" )`  | `true` |
| `is_word`  | `( is_word hello )` | `true` |

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
