#ifndef __LEXER_HPP
#define __LEXER_HPP

#include <filesystem>
#include <fstream>
#include <stack>
#include <vector>

#include "preprocessor.hpp"
#include "util/util.hpp"

// tokenize a document to a vector of Tokens
void tokenize(std::ifstream&, std::vector<Token>&, cwd_stack&);

// tokenize a particular line
void tokenizeLine(std::string&, std::vector<Token>&, line_t, macrodef_map&, cwd_stack&);

#endif