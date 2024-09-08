#ifndef __AST_NODES_HPP
#define __AST_NODES_HPP

#include <string>
#include <vector>

#include "../toolbox.hpp"

enum class ASTNodeType {
    NODE, // base class
    FUNCTION, VARIABLE, RETURN,
    CONDITIONAL, IF_CONDITION, ELSE_IF_CONDITION, ELSE_CONDITION,
    FOR_LOOP, WHILE_LOOP,
    EXPR, UNARY_OP, BIN_OP,
    LIT_BOOL, LIT_CHAR, LIT_DOUBLE, LIT_INT, LIT_NULL
};

// base class for all AST node types
class ASTNode {
    public:
        ASTNode(const Token& token) : err(token.err), raw(token.raw) {};
        virtual ~ASTNode();
        void push(ASTNode* pNode) { children.push_back(pNode); };
        ASTNode* removeChild(size_t i);
        ASTNode* pop() { ASTNode* pNode = lastChild(); children.pop_back(); return pNode; };
        
        virtual ASTNodeType getNodeType() const { return ASTNodeType::NODE; };

        ASTNode* at(unsigned int i) const { return children[i]; };
        size_t size() const { return children.size(); };
        ASTNode* lastChild() const { return this->size() > 0 ? children[this->size()-1] : nullptr; };

        // for error reporting
        ErrInfo err;
        const std::string raw;
    protected:
        std::vector<ASTNode*> children;
};

class ASTReturn : public ASTNode {
    public:
        ASTReturn(const Token& token) : ASTNode(token) {};
        ASTNodeType nodeType() const { return ASTNodeType::RETURN; };
};

/************* CONDITIONALS *************/

class ASTConditional : public ASTNode {
    public:
        ASTConditional(const Token& token) : ASTNode(token) {};
        ~ASTConditional();
        ASTNodeType nodeType() const { return ASTNodeType::CONDITIONAL; };

        void addBranch(ASTNode* pBranch) { branches.push_back(pBranch); };
        const std::vector<ASTNode*> getBranches() const { return branches; };
    private:
        std::vector<ASTNode*> branches;
};

class ASTIfCondition : public ASTNode {
    public:
        ASTIfCondition(const Token& token) : ASTNode(token) {};
        ~ASTIfCondition();
        ASTNodeType nodeType() const { return ASTNodeType::IF_CONDITION; };
        ASTNode* pExpr; // the internal expression for this if-statement
};

class ASTElseIfCondition : public ASTNode {
    public:
        ASTElseIfCondition(const Token& token) : ASTNode(token) {};
        ~ASTElseIfCondition();
        ASTNodeType nodeType() const { return ASTNodeType::ELSE_IF_CONDITION; };
        ASTNode* pExpr; // the internal expression for this else-if-statement
};

class ASTElseCondition : public ASTNode {
    public:
        ASTElseCondition(const Token& token) : ASTNode(token) {};
        ASTNodeType nodeType() const { return ASTNodeType::ELSE_CONDITION; };
};

/************* LOOPS *************/

class ASTForLoop : public ASTNode {
    public:
        ASTForLoop(const Token& token) : ASTNode(token) {};
        ~ASTForLoop();
        ASTNodeType nodeType() const { return ASTNodeType::FOR_LOOP; };
        
        ASTNode* pExprA; // the initial expression
        ASTNode* pExprB; // the condition expression
        ASTNode* pExprC; // the iterating expression
};

class ASTWhileLoop : public ASTNode {
    public:
        ASTWhileLoop(const Token& token) : ASTNode(token) {};
        ~ASTWhileLoop();
        ASTNodeType nodeType() const { return ASTNodeType::WHILE_LOOP; };
        ASTNode* pExpr; // the condition expression
};

/************* OPERATIONS *************/

class ASTExpr : public ASTNode {
    public:
        ASTExpr(const Token& token) : ASTNode(token) {};
        ASTNodeType nodeType() const { return ASTNodeType::EXPR; };
};

class ASTUnaryOp : public ASTNode {
    public:
        ASTUnaryOp(const Token& token) : ASTNode(token), opType(token.type) {};
        ASTNodeType nodeType() const { return ASTNodeType::UNARY_OP; };
        
        ASTNode* right() { return children[0]; };
        TokenType opTokenType() { return opType; };
    private:
        TokenType opType;
};

class ASTBinOp : public ASTNode {
    public:
        ASTBinOp(const Token& token) : ASTNode(token), opType(token.type) {};
        ASTNodeType nodeType() const { return ASTNodeType::BIN_OP; };

        ASTNode* left() { return children[0]; };
        ASTNode* right() { return children[1]; };
        TokenType opTokenType() { return opType; };
    private:
        TokenType opType;
};

/************* LITERALS & IDENTIFIERS *************/

typedef std::pair<std::string, TokenType> param_t;
class ASTFunction : public ASTNode {
    public:
        ASTFunction(const std::string& name, const Token& token) : ASTNode(token), name(name), type(token.type) {};
        ASTNodeType nodeType() const { return ASTNodeType::FUNCTION; };
        void appendParam(const param_t p) {  params.push_back(p);  };

        const std::string& getName() const { return name; };
        TokenType getReturnType() const { return type; };
        std::vector<param_t> getParams() const { return params; };
        size_t getNumParams() const { return params.size(); };
    private:
        std::string name; // name of function
        TokenType type; // return type
        std::vector<param_t> params; // parameters {name, type}
};

class ASTVariable : public ASTNode {
    public:
        ASTVariable(const std::string& name, const Token& token) : ASTNode(token), name(name), type(token.type) {};
        ASTNodeType nodeType() const { return ASTNodeType::VARIABLE; };
    private:
        std::string name; // name of variable
        TokenType type; // type of variable
};

class ASTBoolLiteral : public ASTNode {
    public:
        ASTBoolLiteral(bool val, const Token& token) : ASTNode(token), val(val) {};
        ASTNodeType nodeType() const { return ASTNodeType::LIT_BOOL; };
        bool val;
};

class ASTCharLiteral : public ASTNode {
    public:
        ASTCharLiteral(char val, const Token& token) : ASTNode(token), val(val) {};
        ASTNodeType nodeType() const { return ASTNodeType::LIT_CHAR; };
        char val;
};

class ASTDoubleLiteral : public ASTNode {
    public:
        ASTDoubleLiteral(double val, const Token& token) : ASTNode(token), val(val) {};
        ASTNodeType nodeType() const { return ASTNodeType::LIT_DOUBLE; };
        double val;
};

class ASTIntLiteral : public ASTNode {
    public:
        ASTIntLiteral(int val, const Token& token) : ASTNode(token), val(val) {};
        ASTNodeType nodeType() const { return ASTNodeType::LIT_INT; };
        int val;
};

#endif