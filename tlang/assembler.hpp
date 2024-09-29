#ifndef __ASSEMBLER_HPP
#define __ASSEMBLER_HPP

#include <fstream>
#include <map>
#include <string>

#include "util/scope.hpp"
#include "util/type.hpp"
#include "util/t_exception.hpp"
#include "ast/ast.hpp"
#include "ast/ast_nodes.hpp"

// shorthand used throughout assembler to write with an indent
#define OUT outHandle << TAB

// shorthand used by most binary expressions in assembleExpression
#define OUT_BIN_OP_1A(inst) OUT << #inst << regA << '\n'
#define OUT_BIN_OP_1B(inst) OUT << #inst << regB << '\n'
#define OUT_BIN_OP_2(inst) OUT << #inst << ' ' << regA << ", " << regB << '\n'

// shorthand used in all boolean returns for assembleExpression
#define BIN_OP_RECORD_BOOL OUT << "push AL\n"; scope.addPlaceholder(); resultType = Type(TokenType::TYPE_BOOL)

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
bool assembleBody(ASTNode*, std::ofstream&, Scope&, const std::string&, const bool=true, const bool=false);

// assembles an expression, returning the type of the value pushed to the stack
Type assembleExpression(ASTNode&, std::ofstream&, Scope&);

// implicitly converts a value pushed to the top of the stack to the given type
void implicitCast(std::ofstream&, Type, const Type&, Scope&, const ErrInfo);

#endif