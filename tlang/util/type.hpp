#ifndef __TYPE_HPP
#define __TYPE_HPP

#include <algorithm>
#include <vector>

#include "token.hpp"

#define TYPE_EMPTY_PTR 0

// used to compare and implicitly cast types
class Type; // fwd dec
Type getDominantType(const Type&, const Type&);

class Type {
    public:
        friend Type getDominantType(const Type&, const Type&);

        Type() : primitiveType(TokenType::VOID), pointers() {};
        Type(TokenType prim) : primitiveType(prim), pointers() {};
        Type(TokenType prim, bool isUnsigned) : primitiveType(prim), _isUnsigned(isUnsigned) {};
        
        Type(const Type& type) : primitiveType(type.primitiveType), pointers(type.pointers), _isUnsigned(type._isUnsigned), numArrayHints(type.numArrayHints), forceAsPointer(type.forceAsPointer) {};
        Type(const Type&& type);

        Type& operator=(const Type&);
        Type& operator=(const Type&&);

        bool isUnsigned() const { return _isUnsigned; };

        void addEmptyPointer() { pointers.push_back(TYPE_EMPTY_PTR); forceAsPointer = false; };
        void addHintPointer(size_t);
        void popPointer();
        size_t getNumPointers() const { return pointers.size(); };
        bool isPointer() const { return pointers.size() > 0; };
        void clearPtrs() { pointers.clear(); };

        const std::vector<size_t>& getPointers() const { return pointers; };
        size_t getSizeBytes() const;

        TokenType getPrimitiveType() const { return primitiveType; };

        // returns true if primitive is void and is not a void pointer
        bool isVoidNonPtr() const { return primitiveType == TokenType::VOID && pointers.size() == 0; };

        // returns true if this is a void pointer
        bool isVoidPtr() const { return primitiveType == TokenType::VOID && pointers.size() > 0; };

        // returns true if primitive type is void, regardless of pointers
        bool isVoidAny() const { return primitiveType == TokenType::VOID; };

        bool operator==(const Type&) const;
        bool operator!=(const Type& t) const { return !(*this == t); };

        bool isArray() const { return numArrayHints > 0; };

        size_t getNumArrayHints() const { return numArrayHints; };
        void setNumArrayHints(size_t n) { numArrayHints = n; };
        size_t getArrayHint(size_t i) const { return pointers[i + pointers.size() - numArrayHints]; };
        void setArrayHint(size_t i, size_t v) { pointers[i + pointers.size() - numArrayHints] = v; };

        // clones self but revokes all array hints (for getting the address of this type, hints must be gone to get size right)
        Type getAddressPointer() const;
        void clearArrayHints();

        // used to fix pointer arithmetic
        void setForcedPointer(bool u) { forceAsPointer = u; };
        bool usesForcedPointer() const { return forceAsPointer; };
    private:
        TokenType primitiveType;
        std::vector<size_t> pointers;
        
        bool _isUnsigned = false;

        // for arrays specifically
        size_t numArrayHints = 0;

        // fix for pointer arithmetic
        bool forceAsPointer = false;
};

#endif