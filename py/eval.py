"""Prefix-notation evaluator built on top of parse.py tokenizer."""

from typing import Any, Callable, Dict, List, Optional, Tuple

from parse import TokenType, tokenize


# ---------------------------------------------------------------------------
# Exceptions
# ---------------------------------------------------------------------------

class Return(Exception):
    """Raised to unwind the stack on (return value)."""
    def __init__(self, value: Any) -> None:
        self.value = value


# ---------------------------------------------------------------------------
# AST node types
# ---------------------------------------------------------------------------

class Symbol:
    """A named reference (variable, operator, keyword)."""
    __slots__ = ("name",)
    def __init__(self, name: str) -> None:
        self.name = name

    def __repr__(self) -> str:
        return f"Symbol({self.name!r})"


class SExpr:
    """An S-expression list."""
    __slots__ = ("items",)
    def __init__(self, items: List[Any]) -> None:
        self.items = items

    def __repr__(self) -> str:
        return f"SExpr({self.items!r})"


class Lambda:
    """User-defined function with closure environment."""
    __slots__ = ("params", "body", "closure")
    def __init__(self, params: List[str], body: Any, closure: "Env") -> None:
        self.params = params
        self.body = body
        self.closure = closure

    def __repr__(self) -> str:
        return f"<lambda ({' '.join(self.params)})>"


class Builtin:
    """Built-in callable (binary or unary)."""
    __slots__ = ("func", "arity", "name")
    def __init__(self, func: Callable, arity: int, name: str = "") -> None:
        self.func = func
        self.arity = arity
        self.name = name

    def __repr__(self) -> str:
        return f"<builtin {self.name or '?'}/{self.arity}>"


# ---------------------------------------------------------------------------
# Environment (lexical scoping via parent chain)
# ---------------------------------------------------------------------------

class Env:
    """A scope with bindings and optional parent."""
    __slots__ = ("_bindings", "_parent")

    def __init__(self, parent: Optional["Env"] = None) -> None:
        self._bindings: Dict[str, Any] = {}
        self._parent = parent

    def define(self, name: str, value: Any) -> None:
        """Add binding to *this* scope (shadows parent)."""
        self._bindings[name] = value

    def lookup(self, name: str) -> Any:
        """Walk parent chain to find a binding."""
        if name in self._bindings:
            return self._bindings[name]
        if self._parent is not None:
            return self._parent.lookup(name)
        raise NameError(f"undefined: {name}")

    def set(self, name: str, value: Any) -> None:
        """Mutate existing binding (walks parent chain), or define in
        current scope if not found anywhere."""
        if name in self._bindings:
            self._bindings[name] = value
            return
        if self._parent is not None:
            try:
                self._parent.set(name, value)
                return
            except NameError:
                pass
        self._bindings[name] = value  # define locally

    def extend(self) -> "Env":
        """Create a child env (for function call, do block)."""
        return Env(parent=self)


# ---------------------------------------------------------------------------
# Parser: flat token list → nested AST
# ---------------------------------------------------------------------------

def _token_to_atom(tok: Tuple[TokenType, Any]) -> Any:
    """Convert a single token tuple into an AST atom."""
    kind, value = tok
    if kind == TokenType.WORD:
        return Symbol(value)
    return value  # literal: int, float, bool, None, str


def parse(tokens: List[Tuple[TokenType, Any]]) -> Any:
    """Recursive descent over flat tokens, returning a single AST node."""
    it = iter(enumerate(tokens))

    def _parse_list() -> SExpr:
        items: List[Any] = []
        for _idx, tok in it:
            kind, value = tok
            if kind == TokenType.WORD and value == "(":
                items.append(_parse_list())
            elif kind == TokenType.WORD and value == ")":
                return SExpr(items)
            else:
                items.append(_token_to_atom(tok))
        raise SyntaxError("unexpected EOF inside list")

    # Top-level: wrap in an implicit do-list if multiple top-level forms
    top: List[Any] = []
    for _idx, tok in it:
        kind, value = tok
        if kind == TokenType.WORD and value == "(":
            top.append(_parse_list())
        elif kind == TokenType.WORD and value == ")":
            raise SyntaxError("unexpected ')'")
        else:
            top.append(_token_to_atom(tok))

    if len(top) == 1:
        return top[0]
    return SExpr([Symbol("do")] + top)


# ---------------------------------------------------------------------------
# Built-in operators
# ---------------------------------------------------------------------------

def _make_builtins() -> Dict[str, Builtin]:
    b: Dict[str, Builtin] = {}

    def _bin(name: str, fn: Callable[[Any, Any], Any]) -> None:
        b[name] = Builtin(fn, 2, name)

    def _una(name: str, fn: Callable[[Any], Any]) -> None:
        b[name] = Builtin(fn, 1, name)

    # --- arithmetic ---
    _bin("+", lambda a, b: a + b)
    _bin("-", lambda a, b: a - b)
    _bin("*", lambda a, b: a * b)
    _bin("/", lambda a, b: a / b)
    _bin("mod", lambda a, b: a % b)

    # --- comparison ---
    _bin("<", lambda a, b: a < b)
    _bin(">", lambda a, b: a > b)
    _bin("<=", lambda a, b: a <= b)
    _bin(">=", lambda a, b: a >= b)
    _bin("=", lambda a, b: a == b)
    _bin("!=", lambda a, b: a != b)

    # --- logic ---
    _bin("and", lambda a, b: a and b)
    _bin("or", lambda a, b: a or b)

    # --- unary ---
    _una("not", lambda a: not a)
    _una("neg", lambda a: -a)

    # --- I/O ---
    _una("print", lambda a: print(a, end="") or a)
    _una("println", lambda a: print(a) or a)

    # --- type checks (no '?' suffix — plain WORDs in original tokenizer) ---
    _una("is_null", lambda a: a is None)
    _una("is_bool", lambda a: isinstance(a, bool))
    _una("is_int", lambda a: isinstance(a, int))
    _una("is_float", lambda a: isinstance(a, float))
    _una("is_text", lambda a: isinstance(a, str))
    _una("is_word", lambda a: isinstance(a, str) and a not in ("true", "false", "null"))

    return b


# ---------------------------------------------------------------------------
# Evaluator
# ---------------------------------------------------------------------------

def eval_expr(expr: Any, env: Env) -> Any:
    """Evaluate an AST node in the given environment."""
    # --- Literal values ---
    if isinstance(expr, (int, float, bool, str)) or expr is None:
        return expr

    # --- Symbol lookup ---
    if isinstance(expr, Symbol):
        return env.lookup(expr.name)

    # --- SExpr (S-expression) ---
    if isinstance(expr, SExpr):
        items = expr.items
        if not items:
            return None  # empty list

        first = items[0]

        # Resolve the operator name
        if isinstance(first, Symbol):
            opname = first.name

            # --- Special forms ---

            if opname == "if":
                # (if cond then [else])
                cond = eval_expr(items[1], env)
                if cond:
                    return eval_expr(items[2], env)
                elif len(items) > 3:
                    return eval_expr(items[3], env)
                else:
                    return None

            if opname == "while":
                # (while cond body)
                result: Any = None
                while eval_expr(items[1], env):
                    result = eval_expr(items[2], env)
                return result

            if opname == "do":
                # (do expr1 expr2 ...)  — sequence, return last
                result = None
                for it in items[1:]:
                    result = eval_expr(it, env)
                return result

            if opname == "def":
                # (def name (params) body)
                name_sym = items[1]
                if not isinstance(name_sym, Symbol):
                    raise SyntaxError("def: name must be a symbol")
                params_list = items[2]
                if not isinstance(params_list, SExpr):
                    raise SyntaxError("def: params must be a list")
                param_names = [s.name for s in params_list.items
                               if isinstance(s, Symbol)]
                body = items[3] if len(items) > 3 else None
                fn = Lambda(param_names, body, env)
                env.define(name_sym.name, fn)
                return fn

            if opname == "fun":
                # (fun (params) body)  — anonymous function
                params_list = items[1]
                if not isinstance(params_list, SExpr):
                    raise SyntaxError("fun: params must be a list")
                param_names = [s.name for s in params_list.items
                               if isinstance(s, Symbol)]
                body = items[2] if len(items) > 2 else None
                return Lambda(param_names, body, env)

            if opname == "set":
                # (set name value)
                name_sym = items[1]
                if not isinstance(name_sym, Symbol):
                    raise SyntaxError("set: name must be a symbol")
                val = eval_expr(items[2], env)
                env.set(name_sym.name, val)
                return val

            if opname == "return":
                # (return [value])
                val = eval_expr(items[1], env) if len(items) > 1 else None
                raise Return(val)

        # --- Function application (evaluate operator, then args) ---
        operator = eval_expr(first, env)
        args = [eval_expr(a, env) for a in items[1:]]

        # Built-in
        if isinstance(operator, Builtin):
            if operator.arity == 1:
                if len(args) != 1:
                    raise TypeError(
                        f"{operator.name} expects 1 arg, got {len(args)}")
                return operator.func(args[0])
            elif operator.arity == 2:
                if len(args) != 2:
                    raise TypeError(
                        f"{operator.name} expects 2 args, got {len(args)}")
                return operator.func(args[0], args[1])
            else:
                raise TypeError(f"unsupported builtin arity: {operator.arity}")

        # User-defined lambda
        if isinstance(operator, Lambda):
            return _apply_lambda(operator, args)

        raise TypeError(f"not callable: {operator}")

    raise TypeError(f"cannot evaluate: {expr!r}")


def _apply_lambda(fn: Lambda, args: List[Any]) -> Any:
    """Create a new environment extended from closure, bind params, evaluate body."""
    if len(args) != len(fn.params):
        raise TypeError(
            f"function expects {len(fn.params)} args, got {len(args)}")
    call_env = fn.closure.extend()
    for name, val in zip(fn.params, args):
        call_env.define(name, val)
    try:
        return eval_expr(fn.body, call_env)
    except Return as r:
        return r.value


# ---------------------------------------------------------------------------
# Top-level environment
# ---------------------------------------------------------------------------

def make_global_env() -> Env:
    """Create the global environment with all builtins."""
    env = Env()
    for name, builtin in _make_builtins().items():
        env.define(name, builtin)
    # Also bind true/false/null as literals
    env.define("true", True)
    env.define("false", False)
    env.define("null", None)
    return env


# ---------------------------------------------------------------------------
# Main entry point
# ---------------------------------------------------------------------------

def _main() -> None:
    import sys

    source = open(sys.argv[1]) if len(sys.argv) > 1 else sys.stdin
    text = source.read()

    tokens = tokenize(text)
    if not tokens:
        return

    ast = parse(tokens)
    env = make_global_env()
    try:
        result = eval_expr(ast, env)
        print(result)
    except Exception as e:
        print(f"error: {e}", file=sys.stderr)
        raise


if __name__ == "__main__":
    _main()