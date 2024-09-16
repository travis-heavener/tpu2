#include <stdexcept>
#include <string>
#include <sys/stat.h>

#include "toolbox.hpp"
#include "t_exception.hpp"
#include "type.hpp"

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