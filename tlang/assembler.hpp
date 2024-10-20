#ifndef __ASSEMBLER_HPP
#define __ASSEMBLER_HPP

#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "util/scope.hpp"
#include "util/type.hpp"
#include "util/t_exception.hpp"
#include "ast/ast.hpp"
#include "ast/ast_nodes.hpp"

// shorthand used throughout assembler to write with an indent
#define OUT outHandle << TAB

// shorthand used by most binary expressions in assembleExpression
#define OUT_BIN_OP_1A(inst) OUT << #inst << ' ' << regA << '\n'
#define OUT_BIN_OP_1B(inst) OUT << #inst << ' ' << regB << '\n'
#define OUT_BIN_OP_2(inst) OUT << #inst << ' ' << regA << ", " << regB << '\n'

// shorthand used in all boolean returns for assembleExpression
#define BIN_OP_RECORD_BOOL OUT << "push AL\n"; scope.addPlaceholder(); resultType = Type(TokenType::TYPE_BOOL)

class AssembledFunc {
    public:
        AssembledFunc(const std::string& funcName, const ASTFunction& func);

        const std::string& getName() const { return funcName; };
        const std::string& getStartLabel() const { return startLabel; };
        const std::string& getEndLabel() const { return endLabel; };
        const Type& getReturnType() const { return returnType; };
        const std::vector<Type>& getParamTypes() const { return paramTypes; };
    private:
        std::string funcName, startLabel, endLabel;
        Type returnType;
        std::vector<Type> paramTypes;
};

typedef std::multimap<std::string, AssembledFunc> label_map_t;

// data element class for elements in the .data section
class DataElem {
    public:
        DataElem(const std::string& raw, const std::string& type) :
            raw(raw), type(type) {};
        const std::string raw;
        const std::string type;
};

// generate TPU assembly code from the AST
void generateAssembly(AST&, std::ofstream&);

// for assembling functions
void assembleFunction(ASTFunction&, std::ofstream&);

// for assembling body content that may or may not have its own scope
// returns true if the current body has returned (really only matters in function scopes)
bool assembleBody(ASTNode*, std::ofstream&, Scope&, const AssembledFunc&, const bool=false);

// assembles an expression, returning the type of the value pushed to the stack
Type assembleExpression(ASTNode&, std::ofstream&, Scope&);

// implicitly converts a value pushed to the top of the stack to the given type
void implicitCast(std::ofstream&, Type, Type, Scope&, const ErrInfo);

#endif