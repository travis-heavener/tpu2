#include "type.hpp"

// get the total size taken up on the stack by this type
size_t Type::getStackSizeBytes() const {
    size_t size = getSizeOfType(this->primitiveType);

    for (long long modifier : this->arraySizes) {
        // not specified
        if (modifier == TYPE_NO_ARR_SIZE)
            size *= TYPE_NO_ARR_SIZE < 0 ? -1 : 1;
        size *= modifier;
    }

    return size;
}

bool Type::hasEmptyArrayModifiers() const {
    for (long long modifier : this->arraySizes)
        if (modifier == TYPE_NO_ARR_SIZE)
            return true;

    // base case
    return false;
}

bool Type::operator==(const Type& t) const {
    // check primitive
    if (primitiveType != t.primitiveType)
        return false;

    // check sizes
    if (arraySizes.size() != t.arraySizes.size())
        return false;
    
    // check each modifier
    for (size_t i = 0; i < arraySizes.size(); ++i)
        if (arraySizes[i] != t.arraySizes[i])
            return false;
    
    // base case, match
    return true;
}

bool Type::checkArrayMods(const Type& t) const {
    // check sizes
    if (arraySizes.size() != t.arraySizes.size())
        return false;
    
    // check each modifier
    for (size_t i = 0; i < arraySizes.size(); ++i) {
        if (i == 0) {
            // allow empty, implied only if one has it (left-arg)
            if (arraySizes[i] == TYPE_NO_ARR_SIZE && t.arraySizes[i] == TYPE_NO_ARR_SIZE)
                return false;
            else if (arraySizes[i] != TYPE_NO_ARR_SIZE && t.arraySizes[i] != TYPE_NO_ARR_SIZE && arraySizes[i] != t.arraySizes[i])
                return false;
        } else {
            if (arraySizes[i] != t.arraySizes[i])
                return false;
        }
    }
    
    // base case, match
    return true;
}

// it works out that by just checking the size of the primitive, we can determine
// which type to take. char & bool may be cast as int, and any may be cast as a double
Type Type::checkDominant(Type B) const {
    TokenType primA = this->primitiveType;
    TokenType primB = B.primitiveType;

    if (getSizeOfType(primA) > getSizeOfType(primB))
        return *this;

    // base case, assume B
    return B;
}