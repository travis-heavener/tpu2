#ifndef __PARSER_HPP
#define __PARSER_HPP

#include <vector>

#include "toolbox.hpp"
#include "ast/ast.hpp"

// takes in a vector of Tokens and assembles an AST for the document
AST* parseToAST(const std::vector<Token>&);

#endif