#include "type.hpp"

// used to prevent deprecated-copy warnings (to make copy constuctor work)
// Type& Type::operator=(const Type& t) {
//     this->primitiveType = t.primitiveType;
//     this->arraySizes = t.arraySizes;
//     this->numPtrs = t.numPtrs;
//     this->isLValue = t.isLValue;
//     return *this;
// }

// get the total size taken up in memory by this type
size_t Type::getSizeBytes() const {
    // memory addresses are 2 bytes on a 16-bit system such as this
    size_t size = (this->numPtrs == 0) ? getSizeOfType(this->primitiveType) : 2;

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
    if (primitiveType != t.primitiveType) return false;

    // check num ptrs
    if (numPtrs != t.numPtrs) return false;

    // check sizes
    if (arraySizes.size() != t.arraySizes.size()) return false;
    
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

    // if one is a pointer, just make both a pointer-sized chunk (stored as an int internally)
    if (numPtrs > 0) return *this;
    if (B.numPtrs > 0) return B;

    if (getSizeOfType(primA) > getSizeOfType(primB))
        return *this;

    // else, assume B
    return B;
}