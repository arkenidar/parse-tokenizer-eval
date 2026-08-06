#pragma once

#include <string>
#include <vector>

enum class TokenType
{
    WORD,
    TEXT,
    INTEGER,
    FLOAT,
    BOOLEAN,
    NULL_TYPE
};

struct Token
{
    TokenType kind;
    std::string text; // raw matched text
};

/// Split text by whitespace (honouring double-quoted strings)
/// and return a flat list of Tokens with JSON-like typing.
std::vector<Token> tokenize(const std::string &text);