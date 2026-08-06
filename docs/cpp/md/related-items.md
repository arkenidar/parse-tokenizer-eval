# Related Items

## Concepts closely related to this project

### S-expressions (Symbolic Expressions)

The syntax used by the evaluator: `( operator arg1 arg2 ... )`. Originated with
John McCarthy's Lisp in 1958. The parser here is a minimal recursive descent
over flat tokens indexed by `size_t pos` — no separate lexer/parser separation,
just one shared position counter and a `std::function` lambda for recursion.

### Lexical Scoping

Variables are resolved by walking a chain of environments (`Env` structs linked
via `shared_ptr<Env> parent`), not by dynamic call stack. Functions capture
their definition environment (closure via `Lambda::closure`). This is the same
model used by Scheme, Python, and most modern languages.

### Recursive Descent Parsing

The parser uses a single shared `pos` index and pushes/parses/pops based on
parentheses. No grammar table, no token lookahead beyond the current position.
The `std::function<std::shared_ptr<Value>()>` lambda recursively calls itself on
`(`, returning on `)`. The simplest possible approach for nested structure.

### JSON Type System

The tokenizer's six types (WORD, TEXT, INTEGER, FLOAT, BOOLEAN, NULL_TYPE) map
directly to JSON's type model. `TEXT` is the only addition beyond standard JSON
types — it preserves the distinction between an unquoted symbol and a quoted
string literal.

### Prefix Notation (Polish Notation)

Operators precede their operands: `( + 1 2 )` instead of `1 + 2`. Eliminates the
need for operator precedence rules or parentheses for grouping — nesting is
explicit via S-expression structure.

### Tree-Walking Interpreter

The evaluator is a direct tree walker: it inspects each `Value` node's `tag` via
`is_int()`/`is_sexpr()`/etc. and dispatches based on type. No compilation to
bytecode, no virtual machine. The simplest form of language implementation.

### Environment (Symbol Table)

The `Env` struct is a classic linked-list symbol table using
`std::unordered_map<std::string, std::shared_ptr<Value>>`. Each scope has a
dictionary of bindings and a `shared_ptr<Env> parent` reference. Lookups walk
the chain; definitions only affect the current scope.

### First-Class Functions

Functions are values that can be passed as arguments, returned from other
functions, and stored in variables. The evaluator's `Lambda` objects are
first-class — `( apply-twice double 3 )` works because `double` resolves to a
`Lambda` stored as a `Value` and is passed as an argument.

### Closure

A function plus its defining environment. When
`( def make-adder ( x ) ( fun ( y ) ( + x y ) ) )` is called, the returned
lambda carries `x`'s binding from the call scope via `Lambda::closure` — even
after `make-adder` returns. The non‑owning `shared_ptr` parent chain ensures the
closure environment stays alive.

### Exception-Based Control Flow

`( return value )` throws a C++ `Return` exception (subclass of
`std::exception`). This is caught in `apply_lambda()` to convert it back to a
normal return. It's the same technique used by many interpreter implementations
to avoid manually threading a "should return?" flag through every call.

### Tagged Union / Sum Type

The `Value` struct is a manual implementation of a tagged union (also called a
sum type or discriminated union). An alternative would be C++17's
`std::variant`, but the manual approach gives more control over constructors,
convenience methods, and dispatch readability.

### Reference-Counted Memory Management

All heap objects use `std::shared_ptr<T>`, providing automatic cleanup via
reference counting. This is similar to Python's garbage collection model but
deterministic — objects are destroyed as soon as the last reference drops,
rather than at the next GC cycle.

## Projects and tools in the same space

- **Lisp / Scheme / Racket** — The direct ancestors of this syntax and
  evaluation model.
- **JSON** — The type system inspiration.
- **C++ `<regex>` library** — The single regex doing all tokenization work
  (`std::regex`, `std::sregex_iterator`).
- **C++ `<memory>` library** — `std::shared_ptr`, `std::make_shared`, and the
  aliasing constructor forming the memory management backbone.
- **Make It Lisp (MAL)** — A "make your own Lisp" guide in many languages
  (including C++11).
- **SICP (Structure and Interpretation of Computer Programs)** — The classic
  text on building interpreters and metacircular evaluators.
- **Crafting Interpreters (Robert Nystrom)** — A practical walkthrough of
  building a full interpreter, with a C version covering similar ground.
