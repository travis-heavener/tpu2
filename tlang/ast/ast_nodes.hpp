#ifndef __AST_NODES_HPP
#define __AST_NODES_HPP

#include <string>
#include <vector>

#include "../toolbox.hpp"

enum class ASTNodeType {
    NODE, // base class
    FUNCTION, VARIABLE, RETURN,
    UNARY_OP, BIN_op,
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

/************* OPERATIONS *************/

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
        ASTNodeType nodeType() const { return ASTNodeType::BIN_op; };

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