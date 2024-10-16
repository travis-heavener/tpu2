#ifndef __LEXER_HPP
#define __LEXER_HPP

#include <filesystem>
#include <fstream>
#include <stack>
#include <string>
#include <vector>

#include "preprocessor.hpp"
#include "util/token.hpp"

// tokenize a document to a vector of Tokens
void tokenize(std::ifstream&, std::vector<Token>&, cwd_stack&, const std::string&, const bool=false);

// tokenize a particular line
void tokenizeLine(std::string&, std::vector<Token>&, line_t, cwd_stack&, const std::string&, const bool=false);

#endif