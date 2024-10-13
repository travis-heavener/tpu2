#include "globals.hpp"

/********************************************************/
/*                    STRING METHODS                    */
/********************************************************/

// helper for trimming strings in place
void ltrimString(std::string& str) {
    while (str.length() > 0 && std::isspace(str[0])) {
        str.erase(str.begin(), str.begin()+1);
    }
}

void rtrimString(std::string& str) {
    while (str.length() > 0 && std::isspace(str.back())) {
        str.pop_back();
    }
}

void trimString(std::string& str) {
    ltrimString(str);
    rtrimString(str);
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

// modified from https://stackoverflow.com/a/6296808
bool doesFileExist(const std::string& file) {
    struct stat buf;
    return stat(file.c_str(), &buf) != -1;
}