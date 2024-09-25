#include "type.hpp"

// get the total size taken up in memory by this type
size_t Type::getSizeBytes() const {
    // memory addresses are 2 bytes on a 16-bit system such as this
    size_t size = (pointers.size() == 0) ? getSizeOfType(this->primitiveType) : 2;

    // factor in size hints (from arrays)
    for (long long modifier : pointers)
        if (modifier != TYPE_EMPTY_PTR)
            size *= modifier;

    return size;
}

bool Type::operator==(const Type& t) const {
    // check primitive
    if (primitiveType != t.primitiveType) return false;

    // check num ptrs
    if (pointers.size() != t.pointers.size()) return false;

    // check each modifier
    for (size_t i = 0; i < pointers.size(); ++i)
        if (pointers[i] != t.pointers[i])
            return false;

    // base case, match
    return true;
}