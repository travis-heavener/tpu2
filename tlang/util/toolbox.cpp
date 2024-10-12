#include <stack>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "toolbox.hpp"
#include "t_exception.hpp"
#include "type.hpp"
#include "token.hpp"

// modified from https://stackoverflow.com/a/6296808
bool doesFileExist(const std::string& file) {
    struct stat buf;
    return stat(file.c_str(), &buf) != -1;
}

// true if a character is valid in an identifier
bool isCharValidIdentifier(const char c) {
    return std::isalpha(c) || std::isdigit(c) || c == '_' || 'c' == '$';
}

// true if a character is valid at the start of an identifier
bool isCharValidIdentifierStart(const char c) {
    return isCharValidIdentifier(c) && !std::isdigit(c);
}

// used to expand an escaped character string
char escapeChar(const std::string& str) {
    if (str.size() == 1) return str[0];

    switch (str[1]) {
        case '\'': return '\'';
        case '"': return '"';
        case '\\': return '\\';
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'v': return '\v';
        case '0':
        default: return '\0';
    }
}

void escapeString(std::string& str) {
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\') {
            str[i] = escapeChar(str.substr(i, 2));
            str.erase(str.begin()+i+1, str.begin()+i+2);
        }
    }
}

// helper for trimming strings in place
void ltrimString(std::string& str) {
    while (str.length() > 0 && std::isspace(str[0])) {
        str.erase(str.begin(), str.begin()+1);
    }
}

void rtrimString(std::string& str) {
    while (str.length() > 0 && std::isspace(*str.rbegin())) {
        str.erase(str.end()-1, str.end());
    }
}

void trimString(std::string& str) {
    ltrimString(str);
    rtrimString(str);
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