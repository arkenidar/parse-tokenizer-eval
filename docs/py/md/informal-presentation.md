# Informal Presentation

## The two-file interpreter

I wrote a tokenizer and evaluator that together form a tiny programming
language. The whole thing fits in two Python files — about 440 lines total — and
uses nothing outside the standard library.

## How it started

The original task was simple: split text into words by whitespace, but let
double-quoted strings keep their spaces. Return a flat list where each item
knows its type — word, quoted text, integer, float, boolean, or null. Like a
JSON scanner, but simpler.

So I wrote a single compiled regex with ordered named groups. `re` tries each
alternative left to right, so:

1. `"[^"]*"` grabs quoted text first (capturing the spaces inside)
2. Float pattern before integer so `3.14` doesn't become `3` + `.14`
3. `true`, `false`, `null` as keyword patterns
4. `[^\s]+` catches everything else

That gave me a clean tokenizer that handles the example perfectly:

```
first-word "second word" third_word
→ [(WORD, 'first-word'), (TEXT, 'second word'), (WORD, 'third_word')]
```

## Then I thought — what if this could evaluate too?

If the tokenizer already gives me `(`, `+`, `1`, `2`, `)` as separate WORD and
INTEGER tokens, I just needed a parser that builds nested lists from parentheses
and an evaluator that walks the tree.

The parser is a recursive descent with a shared iterator. `(` pushes into a
nested parse, `)` pops and returns an `SExpr` node. Everything else becomes
either a `Symbol` or a literal value.

The evaluator does prefix notation dispatch. When it sees
`SExpr([Symbol('+'), 1, 2])`, it evaluates `+` (finding a builtin), then
evaluates `1` and `2`, then applies the builtin.

## Why spaced parens?

Early on I tried handling fused syntax like `(+ 1 2)`. The tokenizer's `[^\s]+`
catch-all would produce `WORD: "(+"` — the paren and operator fused together. I
wrote a preprocessor to split them apart. But it felt like fighting the
tokenizer.

The simpler solution: just require spaces around parens. `( + 1 2 )` tokenizes
naturally: `(`, `+`, `1`, `2`, `)` — five clean tokens. The original regex works
perfectly. No preprocessor needed. The convention feels right — like writing a
Lisp with a bit of breathing room.

## The interpreter's personality

It's opinionated in a few ways:

- **Simple, not maximal**: 7 special forms, ~20 builtins. Enough to be useful,
  small enough to read in one sitting.
- **No type system**: values are Python types. Functions are Python objects. The
  environment is a Python dict chain.
- **Return is an exception**: `(return value)` raises `Return(value)`, caught by
  the function application wrapper. Not elegant, but it works and avoids
  threading state through every call.
- **Lexical scope by default**: functions close over their definition
  environment. No surprises.

## What you can do with it

It's a working functional language. Recursion, higher-order functions, closures,
loops, conditionals — the basics are all there. You can define factorial, pass
functions as arguments, iterate with while. Not bad for 440 lines on top of a
regex tokenizer.
