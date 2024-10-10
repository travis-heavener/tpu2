#ifndef __SCOPE_HPP
#define __SCOPE_HPP

#include <string>
#include <vector>

#include "type.hpp"
#include "t_exception.hpp"

#define SCOPE_RETURN_START "0" // identifier cannot be named 0 so this is just a cheeky workaround

// fwd declaration
class Scope;

class ScopeAddr {
    public:
        friend Scope;

        ScopeAddr() : name(""), type(), isAllocated(false) {};
        ScopeAddr(Type type, const std::string& name) : name(name), type(type), isAllocated(true) {};

        std::string name;
        Type type;
        bool isAllocated; // true if taken by a variable, false otherwise (if a placeholder)
};

// used to manage the addresses of assembled variables in the given scope
class Scope {
    public:
        ~Scope() { for (ScopeAddr* pVar : children) delete pVar; }

        bool doesVarExist(const std::string&) const;
        size_t declareVariable(Type, const std::string&, ErrInfo);
        size_t declareFunctionParam(Type, const std::string&, ErrInfo);
        size_t pop();
        void pop(size_t);
        size_t size() const { return children.size(); }
        size_t getOffset(const std::string&, ErrInfo) const;
        ScopeAddr* getVariable(const std::string&, ErrInfo) const;

        void addPlaceholder(size_t=1);
    private:
        std::vector<ScopeAddr*> children;
};

#endif