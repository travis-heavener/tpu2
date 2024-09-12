#ifndef __SCOPE_HPP
#define __SCOPE_HPP

#include <string>
#include <vector>

#include "toolbox.hpp"

// fwd declaration
class Scope;

class ScopeVariable {
    public:
        friend Scope;

        ScopeVariable(TokenType type, const std::string& name) : type(type), name(name) {}
        TokenType getType() const { return type; }
    private:
        TokenType type;
        std::string name;
};

// used to manage the addresses of assembled variables in the given scope
class Scope {
    public:
        ~Scope() { for (ScopeVariable* pVar : children) delete pVar; }

        bool doesVarExist(const std::string&) const;
        size_t declareVariable(TokenType, const std::string&, ErrInfo);
        size_t pop();
        size_t size() const { return children.size(); }
        size_t getSizeBytes() const;
        size_t getSizeBytesOffset() const { return getSizeBytes() + stackPtr; };
        size_t getOffset(const std::string&, ErrInfo) const;
        ScopeVariable* getVariable(const std::string&, ErrInfo) const;

        void incPtr(size_t i) { stackPtr += i; }
        void decPtr(size_t i) { stackPtr -= i; }
    private:
        std::vector<ScopeVariable*> children;
        size_t stackPtr = 0; // stores the value of the current stack pointer
};

#endif