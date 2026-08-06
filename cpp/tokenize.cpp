#include "tokenize.hpp"

#include <regex>

std::vector<Token> tokenize(const std::string &text)
{
    // Ordered regex: quoted text first, then float (before int to avoid
    // partial match), integer, booleans, null, and finally unquoted word.
    static const std::regex tok_re(
        "\"([^\"]*)\"|(-?\\d+\\.\\d+)|(-?\\d+)|(true|false)|(null)|([^\\s]+)");

    std::vector<Token> tokens;

    auto begin = std::sregex_iterator(text.begin(), text.end(), tok_re);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it)
    {
        std::smatch m = *it;

        Token tok;
        if (m[1].matched)
        {
            // "quoted text" — group 1 is the content inside quotes
            tok.kind = TokenType::TEXT;
            tok.text = m[1].str();
        }
        else if (m[2].matched)
        {
            // float literal
            tok.kind = TokenType::FLOAT;
            tok.text = m[2].str();
        }
        else if (m[3].matched)
        {
            // integer literal
            tok.kind = TokenType::INTEGER;
            tok.text = m[3].str();
        }
        else if (m[4].matched)
        {
            // boolean literal
            tok.kind = TokenType::BOOLEAN;
            tok.text = m[4].str();
        }
        else if (m[5].matched)
        {
            // null literal
            tok.kind = TokenType::NULL_TYPE;
            tok.text = m[5].str();
        }
        else if (m[6].matched)
        {
            // unquoted word (catch-all)
            tok.kind = TokenType::WORD;
            tok.text = m[6].str();
        }
        tokens.push_back(tok);
    }

    return tokens;
}