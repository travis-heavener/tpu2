#include "scope.hpp"

bool Scope::doesVarExist(const std::string& name) const {
    for (ScopeVariable* pVar : this->children)
        if (pVar->name == name) return true;
    
    // base case, does not exist
    return false;
}

size_t Scope::declareVariable(TokenType type, const std::string& name) {
    ScopeVariable* pVar = new ScopeVariable(type, name);
    this->children.push_back(pVar);
    return getSizeOfType(type);
}

// pops the last variable off the stack's scope, returning the number of bytes freed
size_t Scope::pop() {
    TokenType type = (*this->children.rbegin())->type;
    this->children.pop_back();
    return getSizeOfType(type);
}