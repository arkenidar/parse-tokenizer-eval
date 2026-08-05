import re
from enum import Enum, auto
from typing import Any, List, Tuple


class TokenType(Enum):
    WORD = auto()      # unquoted identifier/symbol string
    TEXT = auto()      # "quoted text" string (quotes stripped)
    INTEGER = auto()   # integer number
    FLOAT = auto()     # floating-point number
    BOOLEAN = auto()   # true or false
    NULL = auto()      # null


# Ordered regex: quoted text first, then float (before int to avoid
# partial match), integer, booleans, null, and finally unquoted word.
_TOKEN_RE = re.compile(
    r'"(?P<text>[^"]*)"'                # "quoted text"
    r'|(?P<float>-?\d+\.\d+)'          # float literal
    r'|(?P<integer>-?\d+)'             # integer literal
    r'|(?P<boolean>true|false)'        # boolean literal
    r'|(?P<null>null)'                 # null literal
    r'|(?P<word>[^\s]+)'              # unquoted word (catch-all)
)


def tokenize(text: str) -> List[Tuple[TokenType, Any]]:
    """Split text by whitespace (honouring double-quoted strings)
    and return a flat list of (TokenType, value) pairs with JSON-like
    typing."""
    tokens: List[Tuple[TokenType, Any]] = []
    for m in _TOKEN_RE.finditer(text):
        kind = m.lastgroup  # name of the group that matched
        value = m.group(0)  # raw matched text

        if kind == "text":
            tokens.append((TokenType.TEXT, m.group("text")))
        elif kind == "float":
            tokens.append((TokenType.FLOAT, float(value)))
        elif kind == "integer":
            tokens.append((TokenType.INTEGER, int(value)))
        elif kind == "boolean":
            tokens.append((TokenType.BOOLEAN, value == "true"))
        elif kind == "null":
            tokens.append((TokenType.NULL, None))
        elif kind == "word":
            tokens.append((TokenType.WORD, value))
    return tokens


def _main():
    import sys

    source = open(sys.argv[1]) if len(sys.argv) > 1 else sys.stdin
    text = source.read()

    result = tokenize(text)
    print(result)


if __name__ == "__main__":
    _main()