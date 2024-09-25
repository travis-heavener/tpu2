#ifndef __TYPE_HPP
#define __TYPE_HPP

#include <algorithm>
#include <vector>

#include "token.hpp"

// literal types
#define TYPE_EMPTY_PTR 0

class Type {
    public:
        Type() : primitiveType(TokenType::VOID), pointers() {};
        Type(TokenType primitiveType) : primitiveType(primitiveType), pointers() {};

        void addEmptyPointer() { pointers.push_back(TYPE_EMPTY_PTR); };
        void addPointer(size_t n) { pointers.push_back(n); };
        void popPointer() { pointers.pop_back(); };
        size_t getNumPointers() const { return pointers.size(); };
        bool isPointer() const { return pointers.size() > 0; };
        void clearPtrs() { pointers.clear(); };

        const std::vector<size_t>& getPointers() const { return pointers; };
        size_t getSizeBytes() const;

        TokenType getPrimitiveType() const { return primitiveType; };

        bool operator==(const Type&) const;
        bool operator!=(const Type& t) const { return !(*this == t); };
    private:
        TokenType primitiveType;
        std::vector<size_t> pointers;
};

#endif