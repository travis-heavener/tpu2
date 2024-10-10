#include "token.hpp"
#include "toolbox.hpp"
#include "type.hpp"

Type::Type(const Type&& B) {
    this->primitiveType = B.primitiveType;
    this->pointers = std::move(B.pointers);
    this->_isUnsigned = B._isUnsigned;
    this->numArrayHints = B.numArrayHints;
    this->forceAsPointer = B.forceAsPointer;
    this->_isReferencePointer = B._isReferencePointer;
}

Type& Type::operator=(const Type& B) {
    this->primitiveType = B.primitiveType;
    this->pointers = B.pointers;
    this->_isUnsigned = B._isUnsigned;
    this->numArrayHints = B.numArrayHints;
    this->forceAsPointer = B.forceAsPointer;
    return *this;
}

Type& Type::operator=(const Type&& B) {
    this->primitiveType = B.primitiveType;
    this->pointers = std::move(B.pointers);
    this->_isUnsigned = B._isUnsigned;
    this->numArrayHints = B.numArrayHints;
    this->forceAsPointer = B.forceAsPointer;
    return *this;
}

void Type::addHintPointer(size_t n) {
    // insert that way the end is the inner-most subscript when writing
    // ex. int myArr[2][3] is stored as {3, 2}
    const size_t offset = pointers.size() - numArrayHints;
    pointers.insert(pointers.begin() + offset, n);
    ++numArrayHints;
    forceAsPointer = false;
}

void Type::popPointer() {
    // decrement number of stored array hints
    if (numArrayHints > 0) --numArrayHints;
    pointers.pop_back();
    forceAsPointer = false;
}

// get the total size taken up in memory by this type
size_t Type::getSizeBytes(const int opts) const {
    if (isArray() && opts == SIZE_ARR_AS_PTR) return MEM_ADDR_SIZE;
    if (forceAsPointer) return MEM_ADDR_SIZE;

    // get the size of the internal type (whatever is in an array)
    const size_t numPtrs = pointers.size();

    // if this is a POINTER TO AN ARRAY, return a pointer size
    if (numPtrs > 0 && *pointers.rbegin() == TYPE_EMPTY_PTR)
        return MEM_ADDR_SIZE;

    size_t size = (numPtrs > numArrayHints) ? MEM_ADDR_SIZE : getSizeOfType(this->primitiveType);

    for (size_t i = 0; i < numPtrs; ++i)
        // factor in size hints (from arrays)
        if (pointers[i] != TYPE_EMPTY_PTR)
            size *= pointers[i];

    return size;
}

bool Type::operator==(const Type& t) const {
    // check primitive
    if (primitiveType != t.primitiveType) return false;
    if (_isUnsigned != t._isUnsigned) return false;

    // check num ptrs
    if (pointers.size() != t.pointers.size()) return false;

    // check each modifier
    for (size_t i = 0; i < pointers.size(); ++i)
        if (pointers[i] != t.pointers[i])
            return false;

    // base case, match
    return true;
}

bool Type::isParamMatch(const Type& t) const {
    // check primitive
    if (primitiveType != t.primitiveType) return false;
    if (_isUnsigned != t._isUnsigned) return false;

    // check num ptrs
    if (pointers.size() != t.pointers.size()) return false;

    // allow first pointer to be blank, but all others must match
    for (size_t i = pointers.size(); i >= 1; --i) {
        if (pointers[i-1] != t.pointers[i-1] && i < pointers.size())
            return false;
    }

    // base case, matches
    return true;
}

inline unsigned char getPrimitiveTypeRank(TokenType prim, bool isUnsigned) {
    switch (prim) {
        case TokenType::TYPE_CHAR:  return 1 + isUnsigned;
        case TokenType::TYPE_BOOL:  return 2;
        case TokenType::TYPE_INT:   return 3 + isUnsigned;
        case TokenType::TYPE_FLOAT: return 5;
        default: return 0;
    }
}

// used to compare and implicitly cast types
Type getDominantType(const Type& A, const Type& B) {
    /**
     * 1. char
     * 2. unsigned char (aka bool)
     * 3. int
     * 4. unsigned int
     * 5. float
     * 6. pointers (primitive type follows above)
     */
    // if one is a pointer and the other isn't, assume the pointer
    if (A.isPointer() && !B.isPointer()) return A;
    if (!A.isPointer() && B.isPointer()) return B;

    // handle primitives vs pointers
    if (!A.isPointer() && !B.isPointer()) {
        // if primitive types match, prefer unsigned
        if (A.primitiveType == B.primitiveType)
            return A._isUnsigned ? A : B;

        // base case, primitive types do NOT match
        unsigned char rankA = getPrimitiveTypeRank(A.primitiveType, A._isUnsigned);
        unsigned char rankB = getPrimitiveTypeRank(B.primitiveType, B._isUnsigned);

        return (rankA >= rankB) ? A : B;
    } else { // both are pointers (only allowed in bool expressions)
        // assume memory address type (uint16)
        return MEM_ADDR_TYPE;
    }
}

void Type::clearArrayHints() {
    // revoke hints
    for (size_t i = 0; i < pointers.size(); ++i)
        pointers[i] = TYPE_EMPTY_PTR;
    numArrayHints = 0;
}

Type Type::getAddressPointer() const {
    // clone self
    Type clone(*this);
    clone.clearArrayHints(); // revoke hints
    clone.addEmptyPointer(); // add an extra pointer (we have the address of this now)
    return clone;
}