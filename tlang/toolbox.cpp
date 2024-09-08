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