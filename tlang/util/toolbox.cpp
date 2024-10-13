#include <stack>
#include <stdexcept>
#include <string>
#include <vector>

#include "toolbox.hpp"
#include "t_exception.hpp"
#include "type.hpp"
#include "token.hpp"

// true if a character is valid in an identifier
bool isCharValidIdentifier(const char c) {
    return std::isalpha(c) || std::isdigit(c) || c == '_' || 'c' == '$';
}

// true if a character is valid at the start of an identifier
bool isCharValidIdentifierStart(const char c) {
    return isCharValidIdentifier(c) && !std::isdigit(c);
}

// used by the parser to find where a given closing character exists
size_t findClosingParen(const std::vector<Token>& tokens, const size_t start, const size_t end) {
    std::stack<size_t> groupIndices; // only break subexprs that aren't inside parentheticals
    size_t i = start;
    do {
        switch (tokens[i].type) {
            case TokenType::LPAREN:
                groupIndices.push(i);
                break;
            case TokenType::RPAREN:
                groupIndices.pop();
                break;
            default: break;
        }
        ++i;
    } while (groupIndices.size() > 0 && i <= end);

    // verify parenthesis are closed appropriately
    if (groupIndices.size() > 0)
        throw TUnclosedGroupException(tokens[groupIndices.top()].err);

    // skip back once to closing paren
    return i-1;
}

// used by the parser to find where a given closing character exists
size_t findClosingBrace(const std::vector<Token>& tokens, const size_t start, const size_t end) {
    std::stack<size_t> groupIndices; // only break subexprs that aren't inside parentheticals
    size_t i = start;
    do {
        switch (tokens[i].type) {
            case TokenType::LBRACE:
                groupIndices.push(i);
                break;
            case TokenType::RBRACE:
                groupIndices.pop();
                break;
            default: break;
        }
        ++i;
    } while (groupIndices.size() > 0 && i <= end);

    // verify parenthesis are closed appropriately
    if (groupIndices.size() > 0)
        throw TUnclosedGroupException(tokens[groupIndices.top()].err);

    // skip back once to closing paren
    return i-1;
}

void delimitIndices(const std::vector<Token>& tokens, std::vector<size_t>& indices,
                    const size_t start, const size_t end, const TokenType delimiter) {
    // break on delimiters
    for (size_t i = start; i <= end; ++i)
        if (tokens[i].type == delimiter)
            indices.push_back(i);
}