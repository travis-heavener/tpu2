#ifndef __TYPE_HPP
#define __TYPE_HPP

#include <algorithm>
#include <vector>

#include "token.hpp"

// literal types
#define TYPE_EMPTY_PTR 0

class Type {
    public:
        Type() : pointers() {};
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

        bool isArray() const { return numArrayHints > 0; };

        size_t getNumArrayHints() const { return numArrayHints; };
        void setNumArrayHints(size_t n) { numArrayHints = n; };
        size_t getArrayHint(size_t i) const { return pointers[i + pointers.size() - numArrayHints]; };
        void setArrayHint(size_t i, size_t v) { pointers[i + pointers.size() - numArrayHints] = v; };
    private:
        TokenType primitiveType = TokenType::VOID;
        std::vector<size_t> pointers;
        
        // for arrays specifically
        size_t numArrayHints = 0;
};

#endif