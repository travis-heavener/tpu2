#ifndef __PARSER_HPP
#define __PARSER_HPP

#include <vector>

#include "toolbox.hpp"
#include "ast/ast.hpp"

// takes in a vector of Tokens and assembles an AST for the document
AST* parseToAST(const std::vector<Token>&);

/************************ FOR PARSING SPECIFIC ASTNodes ************************/

void parseBody(ASTNode*, const std::vector<Token>&, size_t, size_t);
ASTNode* parseFunction(const std::vector<Token>&, size_t, size_t);
ASTNode* parseExpression(const std::vector<Token>&, size_t, size_t);
ASTNode* parseConditional(const std::vector<Token>&, const std::vector<size_t>&, size_t);

#endif