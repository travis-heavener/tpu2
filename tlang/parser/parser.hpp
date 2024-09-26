#ifndef __PARSER_HPP
#define __PARSER_HPP

#include <vector>

#include "../util/token.hpp"
#include "../util/scope_stack.hpp"
#include "../ast/ast.hpp"

// takes in a vector of Tokens and assembles an AST for the document
AST* parseToAST(const std::vector<Token>&);

/************************ FOR PARSING SPECIFIC ASTNodes ************************/

void parseBody(ASTNode*, const std::vector<Token>&, const size_t, const size_t, scope_stack_t&);
ASTNode* parseFunction(const std::vector<Token>&, const size_t, const size_t, scope_stack_t&);
ASTNode* parseExpression(const std::vector<Token>&, const size_t, const size_t, scope_stack_t&, const bool=false);
ASTNode* parseConditional(const std::vector<Token>&, const std::vector<size_t>&, const size_t, scope_stack_t&);
ASTNode* parseWhileLoop(const std::vector<Token>&, const size_t, const size_t, scope_stack_t&);
ASTNode* parseForLoop(const std::vector<Token>&, const size_t, const size_t, scope_stack_t&);

#endif