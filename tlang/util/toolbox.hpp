#ifndef __TOOLBOX_HPP
#define __TOOLBOX_HPP

#include <string>

// helper to check if file exists
// modified from https://stackoverflow.com/a/6296808
bool doesFileExist(const std::string& file);

// true if a character is valid in an identifier
bool isCharValidIdentifier(const char);

// true if a character is valid at the start of an identifier
bool isCharValidIdentifierStart(const char);

// used to expand an escaped character string
char escapeChar(const std::string&);
void escapeString(std::string&);

// helper for trimming strings in place
void ltrimString(std::string&);
void rtrimString(std::string&);
void trimString(std::string&);

#endif