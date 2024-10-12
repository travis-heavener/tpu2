#ifndef __TOOLBOX_HPP
#define __TOOLBOX_HPP

#include <string>
#include <vector>

#include "token.hpp"
#include "type.hpp"

#define MEM_ADDR_SIZE 2
#define MEM_ADDR_TYPE Type(TokenType::TYPE_INT, true) // uint

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

// used by the parser to find where a given closing character exists
size_t findClosingParen(const std::vector<Token>&, const size_t, const size_t);
size_t findClosingBrace(const std::vector<Token>&, const size_t, const size_t);
void delimitIndices(const std::vector<Token>&, std::vector<size_t>&, const size_t, const size_t, const TokenType=TokenType::COMMA);

#endif