# Related Items

## Concepts closely related to this project

### S-expressions (Symbolic Expressions)

The syntax used by the evaluator: `( operator arg1 arg2 ... )`. Originated with
John McCarthy's Lisp in 1958. The parser here is a minimal recursive descent
over flat tokens — no lexer/parser separation, just one shared iterator.

### Lexical Scoping

Variables are resolved by walking a chain of environments, not by dynamic call
stack. Functions capture their definition environment (closure). This is the
same model used by Scheme, Python, and most modern languages.

### Recursive Descent Parsing

The parser uses a single shared iterator and pushes/parses/pops based on
parentheses. No grammar table, no token lookahead beyond the current position.
The simplest possible approach for nested structure.

### JSON Type System

The tokenizer's six types (word, text, integer, float, boolean, null) map
directly to JSON's type model. `TEXT` is the only addition beyond standard JSON
types — it preserves the distinction between an unquoted symbol and a quoted
string literal.

### Prefix Notation (Polish Notation)

Operators precede their operands: `( + 1 2 )` instead of `1 + 2`. Eliminates the
need for operator precedence rules or parentheses for grouping — nesting is
explicit via S-expression structure.

### Tree-Walking Interpreter

The evaluator is a direct tree walker: it inspects each AST node and dispatches
based on type. No compilation to bytecode, no virtual machine. The simplest form
of language implementation.

### Environment (Symbol Table)

The `Env` class is a classic linked-list symbol table. Each scope has a
dictionary of bindings and a reference to its parent scope. Lookups walk the
chain; definitions only affect the current scope.

### First-Class Functions

Functions are values that can be passed as arguments, returned from other
functions, and stored in variables. The evaluator's `Lambda` objects are
first-class — `( apply-twice double 3 )` works because `double` resolves to a
`Lambda` and is passed as `f`.

### Closure

A function plus its defining environment. When
`( def make-adder ( x )
( fun ( y ) ( + x y ) ) )` is called, the returned
lambda carries `x`'s binding from the call scope — even after `make-adder`
returns.

### Exception-Based Control Flow

`( return value )` raises a Python `Return` exception. This is caught in
`_apply_lambda()` to convert it back to a normal return. It's the same technique
used by many interpreter implementations to avoid manually threading a "should
return?" flag through every call.

## Projects and tools in the same space

- **Lisp / Scheme / Racket** — The direct ancestors of this syntax and
  evaluation model.
- **JSON** — The type system inspiration.
- **Python `re` module** — The single regex doing all tokenization work.
- **Python `ast` module** — CPython's own AST + tree walker, similar
  architecture at a much larger scale.
- **Make It Lisp (MAL)** — A "make your own Lisp" guide in many languages.
- **SICP (Structure and Interpretation of Computer Programs)** — The classic
  text on building interpreters and metacircular evaluators.
