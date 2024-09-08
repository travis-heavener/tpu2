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
        size_t declareVariable(TokenType, const std::string&);
        size_t pop();
    private:
        std::vector<ScopeVariable*> children;
};

#endif