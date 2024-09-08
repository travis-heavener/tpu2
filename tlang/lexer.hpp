#ifndef __LEXER_HPP
#define __LEXER_HPP

#include <fstream>
#include <vector>

#include "toolbox.hpp"

// tokenize a document to a vector of Tokens
void tokenize(std::ifstream&, std::vector<Token>&);

// tokenize a particular line
void tokenizeLine(const std::string&, std::vector<Token>&, line_t);

#endif