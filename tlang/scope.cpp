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

// returns the size of all variables in the scope
size_t Scope::sizeBytes() const {
    size_t size = 0;

    for (size_t i = 0; i < children.size(); i++)
        size += getSizeOfType(children[i]->getType());
    
    return size;
}

// gets the offset address from the end with respect to the rest of the variables below it of a certain variable
size_t Scope::getOffset(const std::string& name, ErrInfo err) const {
    // factor in additional pushes/pops that aren't scope variables (ex. pushes for expressions)
    size_t offset = this->stackPtr - this->sizeBytes();

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