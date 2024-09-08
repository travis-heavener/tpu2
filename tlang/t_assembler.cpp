#include <fstream>
#include <map>
#include <vector>

#include "t_assembler.hpp"
#include "ast/ast.hpp"

#define FUNCTION_MAIN_LABEL main

// generate TPU assembly code from the AST
void generateAssembly(AST& ast, std::ofstream& outHandle) {
    // initialize list of function labels
    size_t nextFunctionID = 0;
    std::map<std::string, std::string> functionLabelMap; // function name, function label

    // iterate over global functions
    std::vector<ASTNode*>& globalChildren = ast.getChildren();
    for (ASTNode* pFunc : globalChildren) {
        // build list of function labels
    }
}