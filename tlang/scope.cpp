#include "scope.hpp"
#include "t_exception.hpp"

bool Scope::doesVarExist(const std::string& name) const {
    for (ScopeVariable* pVar : this->children)
        if (pVar->name == name) return true;
    
    // base case, does not exist
    return false;
}

size_t Scope::declareVariable(TokenType type, const std::string& name, ErrInfo err) {
    if (this->doesVarExist(name))
        throw TIdentifierInUseException(err);
    ScopeVariable* pVar = new ScopeVariable(type, name);
    this->children.push_back(pVar);
    return getSizeOfType(type);
}

// pops the last variable off the stack's scope, returning the number of bytes freed
size_t Scope::pop() {
    TokenType type = (*this->children.rbegin())->type;
    delete *this->children.rbegin();
    this->children.pop_back();
    return getSizeOfType(type);
}

// gets the offset address from the end with respect to the rest of the variables below it of a certain variable
size_t Scope::getOffset(const std::string& name, ErrInfo err) const {
    size_t offset = 0;

    for (long i = children.size()-1; i >= 0; i--) {
        ScopeVariable* pVar = children[i];
        offset += getSizeOfType(pVar->getType());
        if (pVar->name == name) return offset;
    }

    // base case, not found
    throw TUnknownIdentifierException(err);
}

ScopeVariable* Scope::getVariable(const std::string& name, ErrInfo err) const {
    for (ScopeVariable* pVar : this->children)
        if (pVar->name == name) return pVar;

    // base case, not found
    throw TUnknownIdentifierException(err);
}