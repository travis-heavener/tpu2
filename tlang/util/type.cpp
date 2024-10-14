#include "token.hpp"
#include "toolbox.hpp"
#include "type.hpp"

bool doesPrimAImplicitMatchPrimB(const TokenType, const TokenType);

Type::Type(const Type&& B) {
    this->primitiveType = B.primitiveType;
    this->pointers = std::move(B.pointers);
    this->_isUnsigned = B._isUnsigned;
    this->numArrayHints = B.numArrayHints;
    this->_isReferencePointer = B._isReferencePointer;
    this->_isConst = B._isConst;
}

Type& Type::operator=(const Type& B) {
    this->primitiveType = B.primitiveType;
    this->pointers = B.pointers;
    this->_isUnsigned = B._isUnsigned;
    this->numArrayHints = B.numArrayHints;
    this->_isConst = B._isConst;
    return *this;
}

Type& Type::operator=(const Type&& B) {
    this->primitiveType = B.primitiveType;
    this->pointers = std::move(B.pointers);
    this->_isUnsigned = B._isUnsigned;
    this->numArrayHints = B.numArrayHints;
    this->_isConst = B._isConst;
    return *this;
}

void Type::addHintPointer(size_t n) {
    // insert that way the end is the inner-most subscript when writing
    // ex. int myArr[2][3] is stored as {3, 2}
    const size_t offset = pointers.size() - numArrayHints;
    pointers.insert(pointers.begin() + offset, n);
    ++numArrayHints;
}

void Type::popPointer() {
    // decrement number of stored array hints
    if (numArrayHints > 0) --numArrayHints;
    pointers.pop_back();
}

// get the total size taken up in memory by this type
size_t Type::getSizeBytes(const int opts) const {
    if (isArray() && opts == SIZE_ARR_AS_PTR) return MEM_ADDR_SIZE;

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

// returns true if the parameters match, false otherwise
// NOTE: this must be the desired type, and t must be the given type
int Type::isParamMatch(const Type& t, ErrInfo err) const {
    // prevent casting away const qualifiers implicitly
    if (!_isConst && t._isConst) throw TConstQualifierMismatchException(err);

    const TokenType primA = primitiveType;
    const TokenType primB = t.primitiveType;
    const size_t numPtrsA = pointers.size();
    const size_t numPtrsB = t.pointers.size();

    // check if pointers/array pointers match
    // allow first pointer to be blank, but all others must match
    bool doPtrsMatch = numPtrsA == numPtrsB;
    for (size_t i = pointers.size(); i >= 1; --i)
        if (pointers[i-1] != t.pointers[i-1] && i < numPtrsA)
            doPtrsMatch = false;

    /**** check for exact match ****/
    if (primA == primB && _isUnsigned == t._isUnsigned && doPtrsMatch)
        return TYPE_PARAM_EXACT_MATCH;

    /**** check for promotion/implicit conversion ****/

    // allow pointers to be implicitly cast
    if (numPtrsA > 0 && numPtrsB > 0 && !isArray() && !t.isArray())
        return TYPE_PARAM_IMPLICIT_MATCH;

    // allow unsigned and signed of the same type to be converted
    if (primA == primB && numPtrsA == 0 && numPtrsB == 0 && _isUnsigned != t._isUnsigned)
        return TYPE_PARAM_IMPLICIT_MATCH;

    // allow certain primitives to be promoted/implicitly converted
    if (numPtrsA == 0 && numPtrsB == 0 && doesPrimAImplicitMatchPrimB(primA, primB))
        return TYPE_PARAM_IMPLICIT_MATCH;

    // base case, doesn't match
    return TYPE_PARAM_MISMATCH;
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

// returns true when two primitive types match or can be implicitly converted
bool doesPrimAImplicitMatchPrimB(const TokenType A, const TokenType B) {
    if (A == B) return true;

    const bool isAIntegral = A == TokenType::TYPE_CHAR || A == TokenType::TYPE_INT;
    const bool isBIntegral = B == TokenType::TYPE_CHAR || B == TokenType::TYPE_INT;

    // allow integral types to be bools & vice versa
    if ((isAIntegral && B == TokenType::TYPE_BOOL) || (isBIntegral && A == TokenType::TYPE_BOOL))
        return true;

    // allow conversions between integral types themselves
    if (isAIntegral && isBIntegral) return true;

    // base case, can't be implicitly converted
    return false;
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