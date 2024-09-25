#ifndef __TYPE_HPP
#define __TYPE_HPP

#include <algorithm>
#include <vector>

#include "token.hpp"

// literal types
#define TYPE_NO_ARR_SIZE -1
class Type {
    public:
        Type() : primitiveType(TokenType::VOID), arraySizes() {};
        Type(TokenType primitiveType) : primitiveType(primitiveType), arraySizes() {};
        // Type(const Type& t) : primitiveType(t.primitiveType), arraySizes(t.arraySizes), numPtrs(t.numPtrs), isLValue(t.isLValue) {};

        // Type& operator=(const Type&);

        // for adding an array with a specified size (ie. declarations)
        void addArrayModifier(size_t size) { arraySizes.push_back(size); };

        // for adding an array without a specified size (ie. parameter type)
        void addEmptyArrayModifier() { arraySizes.push_back(TYPE_NO_ARR_SIZE); };
        bool hasEmptyArrayModifiers() const;
        void popArrayModifier() { arraySizes.pop_back(); };
        size_t getNumArrayModifiers() const { return arraySizes.size(); };

        void addPointer() { ++numPtrs; };
        void popPointer() { --numPtrs; };
        size_t getNumPtrs() { return numPtrs; };

        bool isArray() const { return arraySizes.size() > 0; };

        Type checkDominant(Type B) const;

        const std::vector<long long>& getArrayModifiers() const { return arraySizes; };
        size_t getSizeBytes() const;

        TokenType getPrimitiveType() const { return primitiveType; };

        bool operator==(const Type&) const;
        bool operator!=(const Type& t) const { return !(*this == t); };
        bool checkArrayMods(const Type& t) const;
        void flipModifiers() { std::reverse(arraySizes.begin(), arraySizes.end()); };

        // NOT used in EQ/NEQ comparison
        bool getIsLValue() const { return isLValue; };
        void setIsLValue(bool lv) { isLValue = lv; };
    private:
        TokenType primitiveType;
        std::vector<long long> arraySizes;
        size_t numPtrs = 0;
        bool isLValue = false;
};

#endif