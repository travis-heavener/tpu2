#ifndef __ASSEMBLER_HPP
#define __ASSEMBLER_HPP

#include <fstream>
#include <map>
#include <string>

#include "util/util.hpp"
#include "ast/ast.hpp"
#include "ast/ast_nodes.hpp"

typedef struct assembled_func_t {
    std::string labelName;
    std::string returnLabel;
    Type returnType;
} assembled_func_t;
typedef std::map<std::string, assembled_func_t> label_map_t;

// generate TPU assembly code from the AST
void generateAssembly(AST&, std::ofstream&);

// for assembling functions
void assembleFunction(ASTFunction&, std::ofstream&);

// for assembling body content that may or may not have its own scope
// returns true if the current body has returned (really only matters in function scopes)
bool assembleBody(ASTNode*, std::ofstream&, Scope&, const std::string&, const bool=true);

// assembles an expression, returning the number of bytes the result uses on the stack
size_t assembleExpression(ASTNode&, std::ofstream&, Scope&, long long=-1);

#endif