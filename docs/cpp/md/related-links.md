# Related Links

## C++ Standard Library

- [`<regex>` — Regular expressions](https://en.cppreference.com/w/cpp/regex) —
  `std::regex`, `std::sregex_iterator`, and `std::smatch` driving the tokenizer.
  Uses ECMAScript grammar by default (same as JavaScript).

- [`<memory>` — Dynamic memory management](https://en.cppreference.com/w/cpp/memory)
  — `std::shared_ptr`, `std::make_shared`, and the aliasing constructor forming
  the memory management backbone. Also `std::enable_shared_from_this`.

- [`<unordered_map>` — Hash table](https://en.cppreference.com/w/cpp/container/unordered_map)
  — The environment's bindings store.
  `std::unordered_map<std::string,
  std::shared_ptr<Value>>`.

- [`<functional>` — Function objects](https://en.cppreference.com/w/cpp/utility/functional)
  — `std::function` enabling the recursive parser lambda. Also
  `std::function<double(double, double)>` for builtin operator lambdas.

- [`<string>` — String library](https://en.cppreference.com/w/cpp/string) —
  `std::string`, `std::to_string`, `std::stoll`, `std::stod` for token parsing.

- [`<cmath>` — Math functions](https://en.cppreference.com/w/cpp/numeric/math) —
  `std::fmod`, `std::floor`, `std::isfinite` for arithmetic builtins.

- [`<stdexcept>` — Standard exceptions](https://en.cppreference.com/w/cpp/error)
  — `std::runtime_error` for evaluation errors. The `Return` class inherits from
  `std::exception`.

- [`<fstream>` / `<sstream>` — File/string I/O](https://en.cppreference.com/w/cpp/io)
  — `std::ifstream` and `std::stringstream` for reading program source from
  files or stdin in `main.cpp`.

- [C++17 Standard (draft)](https://en.cppreference.com/w/cpp/17) — The language
  version used. Key features: `std::shared_ptr` aliasing constructor,
  `std::function` with lambdas, `enum class`, `auto`, range-for.

## Lisp & S-expressions

- [Scheme (R7RS standard)](https://standards.scheme.org/) — The most influential
  minimal Lisp dialect. Lexical scoping, first-class functions, tail calls.
  Direct inspiration for the evaluator's semantics.

- [Racket](https://racket-lang.org/) — A modern Lisp descendant with excellent
  tooling and documentation.

- [Make It Lisp (MAL)](https://github.com/kanaka/mal) — Step-by-step guide to
  implementing a Lisp in dozens of languages (including C++11). Each step adds a
  feature. The evaluator here loosely follows the same progression.

- [SICP — Structure and Interpretation of Computer Programs](https://mitpress.mit.edu/sites/default/files/sicp/index.html)
  — The definitive text on language implementation. Chapter 4 builds a
  metacircular evaluator. Chapter 5 covers compilation.

## JSON

- [JSON specification (RFC 8259)](https://datatracker.ietf.org/doc/html/rfc8259)
  — The type system that inspired the tokenizer's six `TokenType` values.

- [nlohmann/json — JSON for Modern C++](https://github.com/nlohmann/json) — A
  popular C++ JSON library. The `Value` struct here is conceptually similar to
  `nlohmann::json`'s single-type representation but specialized for the Lisp
  evaluator.

## Regular Expressions

- [C++ Regex (cppreference)](https://en.cppreference.com/w/cpp/regex) — Full
  reference for `std::regex`, including ECMAScript syntax and iterator usage.

- [regex101](https://regex101.com/) — Interactive regex tester. Useful for
  experimenting with the tokenizer's pattern (use ECMAScript/JavaScript mode to
  match C++ behavior).

## Parsing

- [Recursive Descent Parsing (Wikipedia)](https://en.wikipedia.org/wiki/Recursive_descent_parser)
  — The parsing technique used by `parse()` in `eval.cpp`. The C++ version uses
  a `std::function` lambda for recursive calls instead of a nested function.

- [Crafting Interpreters (Robert Nystrom)](https://craftinginterpreters.com/) —
  A practical walkthrough of building a full interpreter in C and Java. Covers
  scanning, parsing, ASTs, and tree-walking evaluation. The C version (`clox`)
  is particularly relevant.

## C++ Memory Management References

- [`std::shared_ptr` aliasing constructor](https://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr)
  — Constructor (8): the non‑owning aliasing pattern used by `Env::extend()`.

- [`std::enable_shared_from_this`](https://en.cppreference.com/w/cpp/memory/enable_shared_from_this)
  — The standard library's mechanism for obtaining a `shared_ptr` from `this`.
  Related to but subtly different from the aliasing pattern used here.

- [C++ Core Guidelines — Resource Management](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-resource)
  — Best practices for `shared_ptr`, `unique_ptr`, and RAII in modern C++.
