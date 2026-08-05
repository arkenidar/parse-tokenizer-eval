# Related Links

## Python Standard Library

- [re — Regular expression operations](https://docs.python.org/3/library/re.html)
  — The single `re.compile()` + `finditer()` pattern driving the tokenizer.

- [enum — Support for enumerations](https://docs.python.org/3/library/enum.html)
  — `TokenType` enum (`WORD`, `TEXT`, `INTEGER`, `FLOAT`, `BOOLEAN`, `NULL`).

- [typing — Support for type hints](https://docs.python.org/3/library/typing.html)
  — `List`, `Tuple`, `Dict`, `Optional`, `Callable`, `Any`.

- [ast — Abstract Syntax Trees](https://docs.python.org/3/library/ast.html) —
  CPython's own AST module, similar tree-walking architecture.

## Lisp & S-expressions

- [Scheme (R7RS standard)](https://standards.scheme.org/) — The most influential
  minimal Lisp dialect. Lexical scoping, first-class functions, tail calls.
  Direct inspiration for the evaluator's semantics.

- [Racket](https://racket-lang.org/) — A modern Lisp descendant with excellent
  tooling and documentation.

- [Make It Lisp (MAL)](https://github.com/kanaka/mal) — Step-by-step guide to
  implementing a Lisp in dozens of languages. Each step adds a feature. The
  evaluator here loosely follows the same progression.

- [SICP — Structure and Interpretation of Computer Programs](https://mitpress.mit.edu/sites/default/files/sicp/index.html)
  — The definitive text on language implementation. Chapter 4 builds a
  metacircular evaluator. Chapter 5 covers compilation.

## JSON

- [JSON specification (RFC 8259)](https://datatracker.ietf.org/doc/html/rfc8259)
  — The type system that inspired the tokenizer's six `TokenType` values.

- [json — Python JSON encoder/decoder](https://docs.python.org/3/library/json.html)
  — Python's built-in JSON support. The tokenizer here is intentionally simpler
  (no object/array nesting in the tokenizer — that's the evaluator's job).

## Regular Expressions

- [Regular Expression HOWTO (Python)](https://docs.python.org/3/howto/regex.html)
  — Python-specific regex guide covering named groups `(?P<name>...)`, which are
  central to the tokenizer's dispatch.

- [regex101](https://regex101.com/) — Interactive regex tester. Useful for
  experimenting with the tokenizer's pattern.

## Parsing

- [Recursive Descent Parsing (Wikipedia)](https://en.wikipedia.org/wiki/Recursive_descent_parser)
  — The parsing technique used by `parse()` in `eval.py`.

- [Crafting Interpreters (Robert Nystrom)](https://craftinginterpreters.com/) —
  A practical walkthrough of building a full interpreter in C and Java. Covers
  scanning, parsing, ASTs, and tree-walking evaluation.
