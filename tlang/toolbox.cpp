#include <string>
#include <sys/stat.h>

#include "toolbox.hpp"

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

// returns true if the given TokenType is that of a type name (ex. TYPE_INT)
bool isTokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::TYPE_INT:
        case TokenType::TYPE_DOUBLE:
        case TokenType::TYPE_CHAR:
        case TokenType::TYPE_BOOL:
        case TokenType::VOID:
            return true;
        default:
            return false;
    }
}