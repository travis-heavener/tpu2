#ifndef __PARSER_PRECEDENCES_HPP
#define __PARSER_PRECEDENCES_HPP

#include <vector>

#include "../../util/globals.hpp"
#include "../util/token.hpp"
#include "../util/scope_stack.hpp"
#include "../ast/ast_nodes.hpp"

// an abstraction from the parser to make precedence parsing more straight forward

// order of operations for expression parsing
// operator precedence for C: https://en.cppreference.com/w/c/language/operator_precedence

void parsePrecedence1(const std::vector<Token>&, size_t, size_t, ASTNode*, scope_stack_t&);
void parsePrecedence2(const std::vector<Token>&, ASTNode*);
void parsePrecedence3(ASTNode*);
void parsePrecedence4(ASTNode*);
void parsePrecedence5(ASTNode*);
void parsePrecedence6(ASTNode*);
void parsePrecedence7(ASTNode*);
void parsePrecedence8(ASTNode*);
void parsePrecedence9(ASTNode*);
void parsePrecedence10(ASTNode*);
void parsePrecedence11(ASTNode*);
void parsePrecedence12(ASTNode*);
void parsePrecedence14(ASTNode*);

#endif