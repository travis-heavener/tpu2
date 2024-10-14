#ifndef __SCOPE_STACK_HPP
#define __SCOPE_STACK_HPP

#include <map>
#include <string>
#include <vector>

#include "type.hpp"
#include "t_exception.hpp"

// for parser scope stacks
#define PARSER_VAR_NO_PARENT -1

// fwd decs
class AST;
class ASTNode;
class ParserVariable {
    public:
        ParserVariable(Type type) : type(type), pParent(nullptr), pVarDecNode(nullptr) {};
        ParserVariable(Type type, ASTNode* pParent, void* pVarDecNode)
            : type(type), pParent(pParent), pVarDecNode(pVarDecNode) {};

        void remove();

        Type type;
        bool isUnused = true;
        ASTNode* pParent;
        void* pVarDecNode;
};

class ParserFunction {
    public:
        ParserFunction(Type type, bool isMain, AST* pParent, void* pFuncNode, std::vector<Type>& paramTypes) :
            type(type), isMainFunction(isMain), pParent(pParent), pFuncNode(pFuncNode), paramTypes(paramTypes) {};

        int doParamsMatch(const std::vector<Type>&, const ErrInfo);
        void remove();

        Type type;
        bool isUnused = true;

        const std::vector<Type>& getParamTypes() const { return paramTypes; };
    private:
        bool isMainFunction;
        AST* pParent;
        void* pFuncNode;
        std::vector<Type> paramTypes;
};

class ParserScope {
    public:
        ~ParserScope();

        bool isNameTaken(const std::string& name) const { return functions.count(name) > 0 || isVarNameTaken(name); };

        bool isVarNameTaken(const std::string& name) const { return variables.count(name) > 0; };

        ParserVariable* getVariable(const std::string& name) { return isVarNameTaken(name) ? variables[name] : nullptr; };

        std::map<std::string, ParserVariable*> variables;
        std::multimap<std::string, ParserFunction*> functions;
};

typedef std::vector<ParserScope*> scope_stack_t;

// lookup variable from scope stack
ParserVariable* lookupParserVariable(scope_stack_t&, const std::string&, ErrInfo);

// lookup function from scope stack
ParserFunction* lookupParserFunction(scope_stack_t&, const std::string&, ErrInfo, const std::vector<Type>&, int&);

// declare a variable in the immediate scope
void declareParserVariable(scope_stack_t&, const std::string&, ParserVariable*, ErrInfo);

// declare a function in the immediate scope
void declareParserFunction(scope_stack_t&, const std::string&, ParserFunction*, const std::vector<Type>&, ErrInfo);

// used to pop off a scope stack
void popScopeStack(scope_stack_t&);

#endif