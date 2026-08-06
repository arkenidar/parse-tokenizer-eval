# Informal Presentation

## The C++ port

I ported the two‑file Python interpreter to C++17. The whole thing fits in six
files (two headers, three implementation files, and a Makefile) — about 900
lines total — and uses nothing outside the C++ standard library.

## Why C++?

The Python version works fine, but C++ brings a few things to the table:

- **Compiled to native code** — faster startup and execution, no interpreter
  overhead
- **Type safety at compile time** — the `Value` tagged union catches mismatched
  tag accesses in code review rather than at runtime
- **Explicit memory model** — `shared_ptr` shows you exactly where ownership
  lives, unlike Python's pervasive GC

## How the tokenizer works in C++

The regex is identical to the Python one — same ordering, same capture groups.
But in C++ there are no named groups, so we match by index:

```cpp
static const std::regex tok_re(
    "\"([^\"]*)\"|(-?\\d+\\.\\d+)|(-?\\d+)|(true|false)|(null)|([^\\s]+)");
```

`std::sregex_iterator` walks the matches, and we check `m[1].matched` through
`m[6].matched` to dispatch to the right `TokenType`.

The main difference from Python: `Token::text` stores the raw string for all
token kinds. Conversion to typed values (`std::stoll`, `std::stod`, `Symbol`
wrapping) happens later in `token_to_atom()` inside `eval.cpp`. This keeps the
tokenizer truly agnostic about its downstream consumer.

## The tagged union

Python just returns `Any` and checks `isinstance()`. In C++ we need a typed
container. I chose a manual tagged union (`Value` with `Tag` enum + union
fields) over `std::variant` for a few reasons:

- **Explicit constructors** — `Value(42)` unambiguously creates `Tag::INT`,
  `Value("hello")` creates `Tag::TEXT`, `Value(nullptr)` creates `Tag::NIL`.
  `std::variant` would need `Value(std::in_place_type<int64_t>, 42)` or similar
  ceremony.
- **Convenience methods** — `is_int()`, `truthy()`, `eq()` live right on the
  struct. With `std::variant` you'd need free functions or a separate wrapper.
- **No visitation needed** — a simple `switch (tag)` dispatches in the
  evaluator, which is more readable than `std::visit` with overloaded lambdas.

The cost is manual discipline: you must check `tag` before accessing
`int_val`/`float_val`/etc. But the `is_*()` helpers make this painless.

## The parser's recursive lambda

Python's parser uses a nested function `_parse_list()` with `nonlocal` access to
the iterator. In C++ we can't nest functions, but we can capture everything in a
`std::function` lambda:

```cpp
size_t pos = 0;
std::function<std::shared_ptr<Value>()> parse_list;
parse_list = [&]() -> std::shared_ptr<Value> {
    // ... walks tokens[pos] ...
    // calls parse_list() recursively on '('
};
```

The `[&]` capture gives the lambda access to `tokens` and `pos` from the
enclosing scope. The `std::function` type erasure is needed because the lambda
refers to itself — you can't capture a lambda by reference before it's fully
typed.

## The non‑owning parent trick

Lexical scoping means each function call creates a child environment whose
parent is the closure scope. The parent needs to stay alive as long as any child
references it, but the child shouldn't own the parent (or we'd have reference
cycles).

Python's GC handles cycles automatically. In C++, `shared_ptr` would leak. The
solution: the aliasing constructor:

```cpp
std::shared_ptr<Env> Env::extend()
{
    return std::make_shared<Env>(
        std::shared_ptr<Env>(this, [](Env *) {}));
}
```

The inner `shared_ptr` points to `this` (the current `Env`) but has an empty
deleter — it never calls `delete`. The outer `make_shared` creates the child
`Env`, storing this non‑owning pointer as `parent`. When all children are gone,
the reference count on the aliasing `shared_ptr` drops to zero, the empty
deleter runs (doing nothing), and the parent can be cleaned up by its actual
`shared_ptr` owners.

It's a well‑known pattern — `std::enable_shared_from_this` is the standard
library version of the same idea, but it can't express the "non‑owning"
semantics directly.

## The interpreter's personality

- **The same language, different runtime** — all the special forms, builtins,
  and syntax work identically to the Python version. You can run the same `.txt`
  program files through either interpreter.
- **Explicit number promotion** — `as_double()` and `num_result()` handle the
  int/float boundary. `( + 1 2 )` stays `int`, but `( / 3 2 )` becomes `float`
  `1.5`. The rules are the same as Python.
- **Return is still an exception** — `(return value)` throws `Return`, caught by
  `apply_lambda()`. In C++ this is a `std::exception` subclass with a
  `shared_ptr<Value>` payload.
- **Stdlib‑only** — `<regex>`, `<memory>`, `<unordered_map>`, `<functional>`,
  `<cmath>`, `<fstream>`, `<sstream>`. That's it. Compile with any C++17
  compiler.

## What you can do with it

Everything the Python version can do: recursion, higher‑order functions,
closures, loops, conditionals. Same factorial, same `apply‑twice`, same
while‑sum patterns. Just compiled to native code with explicit memory
management.
