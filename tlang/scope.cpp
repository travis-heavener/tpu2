#include "scope.hpp"
#include "t_exception.hpp"

bool Scope::doesVarExist(const std::string& name) const {
    for (ScopeAddr* pVar : this->children)
        if (pVar->isAllocated && pVar->name == name) return true;

    // base case, does not exist
    return false;
}

size_t Scope::declareVariable(TokenType type, const std::string& name, ErrInfo err) {
    if (this->doesVarExist(name))
        throw TIdentifierInUseException(err);
    ScopeAddr* pVar = new ScopeAddr(type, name);
    this->children.push_back(pVar);

    size_t size = getSizeOfType(type);
    for (size_t i = 0; i < size-1; i++) // add remaining placeholders
        this->addPlaceholder();
    return size;
}

// pops the last variable off the stack's scope, returning the number of bytes freed
size_t Scope::pop() {
    ScopeAddr* pVar = *this->children.rbegin();
    size_t size = pVar->isAllocated ? getSizeOfType(pVar->type) : 1;

    for (size_t i = 0; i < size; i++) {
        delete *this->children.rbegin();
        this->children.pop_back();
    }

    return size;
}

// gets the offset address from the end with respect to the rest of the variables below it of a certain variable
size_t Scope::getOffset(const std::string& name, ErrInfo err) const {
    size_t offset = 0;

    for (long i = children.size()-1; i >= 0; i--) {
        ScopeAddr* pVar = children[i];
        offset++;
        if (pVar->isAllocated && pVar->name == name) return offset;
    }

    // base case, not found
    throw TUnknownIdentifierException(err);
}

ScopeAddr* Scope::getVariable(const std::string& name, ErrInfo err) const {
    for (ScopeAddr* pVar : this->children)
        if (pVar->isAllocated && pVar->name == name) return pVar;

    // base case, not found
    throw TUnknownIdentifierException(err);
}