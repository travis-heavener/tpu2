#include "scope.hpp"
#include "t_exception.hpp"

bool Scope::doesVarExist(const std::string& name) const {
    for (ScopeAddr* pVar : this->children)
        if (pVar->isAllocated && pVar->name == name) return true;

    // base case, does not exist
    return false;
}

size_t Scope::declareVariable(Type type, const std::string& name, ErrInfo err) {
    if (this->doesVarExist(name))
        throw TIdentifierInUseException(err);
    ScopeAddr* pVar = new ScopeAddr(type, name);
    this->children.push_back(pVar);

    size_t size = type.getSizeBytes();
    for (size_t i = 0; i < size-1; i++) // add remaining placeholders
        this->addPlaceholder();
    return size;
}

// handles any array as a pointer
size_t Scope::declareFunctionParam(Type type, const std::string& name, ErrInfo err) {
    if (type.isArray()) {
        // pass any arrays by reference by pushing a pointer
        // and dropping all array modifiers (mere suggestions)
        size_t numArrayMods = type.getNumArrayModifiers();
        for (size_t i = 0; i < numArrayMods; ++i)
            type.popArrayModifier();

        type.addPointer();
        return declareVariable(type, name, err);
    }

    // base case, not an array, pass by value
    return declareVariable(type, name, err);
}

// pops the last variable off the stack's scope, returning the number of bytes freed
size_t Scope::pop() {
    delete *this->children.rbegin();
    this->children.pop_back();
    return 1;
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

void Scope::addPlaceholder(size_t n) {
    for (size_t i = 0; i < n; ++i)
        children.push_back(new ScopeAddr());
}