#include <string>

#include "../ast/ast.hpp"
#include "../ast/ast_nodes.hpp"
#include "scope_stack.hpp"
#include "type.hpp"
#include "t_exception.hpp"
#include "config.hpp"

// free all children
ParserScope::~ParserScope() {
    for (auto [name, pVar] : variables)
        delete pVar;

    for (auto [name, pFunc] : functions)
        delete pFunc;
}

// removes the current referenced node from the parent
void ParserVariable::remove() {
    // if pParent is a nullptr, it can't be removed (ex. function args)
    if (pParent != nullptr)
        pParent->removeByAddress(pVarDecNode);
};

// removes the current referenced node from the parent
void ParserFunction::remove() {
    // if this is a main function, don't remove
    if (!isMainFunction)
        pParent->removeByAddress(pFuncNode);
};

// returns true when the parameters match on two ParserFunctions
bool ParserFunction::doParamsMatch(const std::vector<Type>& paramsB) {
    if (paramsB.size() != paramTypes.size()) return false;

    for (size_t i = 0; i < paramTypes.size(); ++i) {
        if (!paramTypes[i].isParamMatch( paramsB[i] )) {
            return false;
        }
    }

    // base case, match
    return true;
}

// lookup variable from scope stack
ParserVariable* lookupParserVariable(scope_stack_t& scopeStack, const std::string& name, ErrInfo err) {
    // look in the stack, up
    auto itr = scopeStack.rbegin();
    for ((void)itr; itr != scopeStack.rend(); ++itr) {
        // check this scope
        ParserScope* pScope = *itr;
        ParserVariable* pVar = pScope->getVariable(name);
        if (pVar != nullptr) {
            pVar->isUnused = false;
            return pVar;
        }
    }

    // base case, not found
    throw TUnknownIdentifierException(err);
}

// lookup function from scope stack
ParserFunction* lookupParserFunction(scope_stack_t& scopeStack, const std::string& name, ErrInfo err, const std::vector<Type>& paramTypes) {
    // look in global scope
    ParserScope* pScope = scopeStack[0];

    // search functions in global scope
    auto range = pScope->functions.equal_range(name);
    for (auto itr = range.first; itr != range.second; ++itr) {
        // check parameters
        ParserFunction* pParserFunc = itr->second;
        if (pParserFunc->doParamsMatch(paramTypes)) {
            pParserFunc->isUnused = false;
            return pParserFunc;
        }
    }

    // base case, not found
    throw TUnknownIdentifierException(err);
}

// declare a variable in the immediate scope
void declareParserVariable(scope_stack_t& scopeStack, const std::string& name, ParserVariable* pParserVar, ErrInfo err) {
    // verify this variable isn't already defined in the immediate scope
    ParserScope* pScope = scopeStack.back();
    if (pScope->isVarNameTaken(name))
        throw TIdentifierInUseException(err);

    // declare variable
    pScope->variables.insert({name, pParserVar});
}

// declare a function in the immediate scope
void declareParserFunction(scope_stack_t& scopeStack, const std::string& name, ParserFunction* pParserFunc, const std::vector<Type>& paramTypes, ErrInfo err) {
    // verify this function isn't already defined in the global scope
    ParserScope* pScope = scopeStack[0];

    // search functions in global scope
    auto range = pScope->functions.equal_range(name);
    for (auto itr = range.first; itr != range.second; ++itr)
        if (itr->second->doParamsMatch(paramTypes)) // check parameters
            throw TIdentifierInUseException(err);

    // declare function
    pScope->functions.insert({name, pParserFunc});
}

// used to pop off a scope stack
void popScopeStack(scope_stack_t& scopeStack) {
    // remove any unused variables from AST
    ParserScope* pScope = scopeStack.back();

    // remove unused variables
    if (DELETE_UNUSED_VARIABLES) {
        for (auto [name, pParserVar] : pScope->variables) {
            if (pParserVar->isUnused)
                pParserVar->remove(); // remove from ASTNode parent
        }
    }

    // remove unused functions
    if (DELETE_UNUSED_FUNCTIONS) {
        for (auto [name, pParserFunc] : pScope->functions) {
            if (pParserFunc->isUnused)
                pParserFunc->remove(); // remove from AST
        }
    }

    // pop scope stack
    delete pScope;
    scopeStack.pop_back();
}