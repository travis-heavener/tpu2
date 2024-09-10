#ifndef __T_ASSEMBLER_HPP
#define __T_ASSEMBLER_HPP

#include <fstream>
#include <map>
#include <string>

#include "scope.hpp"
#include "ast/ast.hpp"
#include "ast/ast_nodes.hpp"

typedef std::map<std::string, std::string> label_map_t;

// generate TPU assembly code from the AST
void generateAssembly(AST&, std::ofstream&);

// abstractions from generateAssembly for visual bliss
void assembleBody(ASTNode*, std::ofstream&, label_map_t&, Scope&, const std::string&, const bool);

// assembles an expression, returning the number of bytes the result uses on the stack
size_t assembleExpression(ASTNode&, std::ofstream&, label_map_t&, Scope&);

#endif