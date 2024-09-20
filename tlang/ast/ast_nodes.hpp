#ifndef __AST_NODES_HPP
#define __AST_NODES_HPP

#include <string>
#include <vector>

#include "../util/util.hpp"

enum class ASTNodeType {
    NODE, // base class
    FUNCTION, FUNCTION_CALL, VAR_DECLARATION, IDENTIFIER, RETURN,
    CONDITIONAL, IF_CONDITION, ELSE_IF_CONDITION, ELSE_CONDITION,
    FOR_LOOP, WHILE_LOOP,
    EXPR, UNARY_OP, BIN_OP, TYPE_CAST, SIZEOF,
    LIT_BOOL, LIT_CHAR, LIT_FLOAT, LIT_INT, LIT_VOID, LIT_ARR, ARR_SUBSCRIPT
};

enum class ASTUnaryType {
    BASE,
    POST_INC,
    POST_DEC,
    PRE_INC,
    PRE_DEC,
    TYPE_CAST,
    SIZEOF
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
        bool isAssembled = false;
    protected:
        std::vector<ASTNode*> children;
};

class ASTReturn : public ASTNode {
    public:
        ASTReturn(const Token& token) : ASTNode(token) {};
        ASTNodeType getNodeType() const { return ASTNodeType::RETURN; };
};

/************* CONDITIONALS *************/

class ASTConditional : public ASTNode {
    public:
        ASTConditional(const Token& token) : ASTNode(token) {};
        ASTNodeType getNodeType() const { return ASTNodeType::CONDITIONAL; };
};

class ASTIfCondition : public ASTNode {
    public:
        ASTIfCondition(const Token& token) : ASTNode(token) {};
        ~ASTIfCondition();
        ASTNodeType getNodeType() const { return ASTNodeType::IF_CONDITION; };
        ASTNode* pExpr = nullptr; // the internal expression for this if-statement
};

class ASTElseIfCondition : public ASTNode {
    public:
        ASTElseIfCondition(const Token& token) : ASTNode(token) {};
        ~ASTElseIfCondition();
        ASTNodeType getNodeType() const { return ASTNodeType::ELSE_IF_CONDITION; };
        ASTNode* pExpr = nullptr; // the internal expression for this else-if-statement
};

class ASTElseCondition : public ASTNode {
    public:
        ASTElseCondition(const Token& token) : ASTNode(token) {};
        ASTNodeType getNodeType() const { return ASTNodeType::ELSE_CONDITION; };
};

/************* LOOPS *************/

class ASTForLoop : public ASTNode {
    public:
        ASTForLoop(const Token& token) : ASTNode(token) {};
        ~ASTForLoop();
        ASTNodeType getNodeType() const { return ASTNodeType::FOR_LOOP; };
        
        ASTNode* pExprA = nullptr; // the initial expression
        ASTNode* pExprB = nullptr; // the condition expression
        ASTNode* pExprC = nullptr; // the iterating expression
};

class ASTWhileLoop : public ASTNode {
    public:
        ASTWhileLoop(const Token& token) : ASTNode(token) {};
        ~ASTWhileLoop();
        ASTNodeType getNodeType() const { return ASTNodeType::WHILE_LOOP; };
        ASTNode* pExpr = nullptr; // the condition expression
};

/************* OPERATIONS *************/

class ASTExpr : public ASTNode {
    public:
        ASTExpr(const Token& token) : ASTNode(token) {};
        ASTNodeType getNodeType() const { return ASTNodeType::EXPR; };
        
        Type inferType(scope_stack_t& scopeStack) const;
        Type type;
};

class ASTOperator : public ASTNode {
    public:
        ASTOperator(const Token& token, bool isUnary) : ASTNode(token), opType(token.type), isUnary(isUnary) {}
        ASTNodeType getNodeType() const { return isUnary ? ASTNodeType::UNARY_OP : ASTNodeType::BIN_OP; }
        
        ASTNode* left() { return children[0]; }
        ASTNode* right() { return children[1]; }
        TokenType getOpTokenType() const { return opType; }

        void setIsUnary(bool isUnary) { this->isUnary = isUnary; }
        bool getIsUnary() const { return this->isUnary; }

        void setUnaryType(ASTUnaryType t) { this->unaryType = t; };
        ASTUnaryType getUnaryType() const { return this->unaryType; };

        void determineResultType(scope_stack_t&);
        Type getResultType() const { return returnType; };
    private:
        ASTUnaryType unaryType = ASTUnaryType::BASE;
        TokenType opType;
        Type returnType;
        bool isUnary;
};

/************* LITERALS & IDENTIFIERS *************/

class ASTFuncParam {
    public:
        ASTFuncParam(const std::string& name, Type type) : name(name), type(type) {};
        std::string name;
        Type type;
};

class ASTFunction : public ASTNode {
    public:
        ASTFunction(const std::string& name, const Token& token, Type type) : ASTNode(token), name(name), type(type), params() {};
        ~ASTFunction();
        ASTNodeType getNodeType() const { return ASTNodeType::FUNCTION; };
        void appendParam(ASTFuncParam* p) {  params.push_back(p);  };

        const std::string& getName() const { return name; };
        Type getReturnType() const { return type; };
        ASTFuncParam* paramAt(size_t i) { return params[i]; };
        size_t getNumParams() const { return params.size(); };
    private:
        std::string name; // name of function
        Type type; // return type
        std::vector<ASTFuncParam*> params; // parameters {name, type}
};

class ASTFunctionCall : public ASTNode {
    public:
        ASTFunctionCall(const Token& token) : ASTNode(token) {};
        ASTNodeType getNodeType() const { return ASTNodeType::FUNCTION_CALL; };
};

class ASTArraySubscript : public ASTNode {
    public:
        ASTArraySubscript(const Token& token) : ASTNode(token) {};
        ASTNodeType getNodeType() const { return ASTNodeType::ARR_SUBSCRIPT; };
};

class ASTIdentifier : public ASTNode {
    public:
        ASTIdentifier(const Token& token, bool inAssign) : ASTNode(token), isInAssignExpr(inAssign) {};
        ~ASTIdentifier();
        ASTNodeType getNodeType() const { return ASTNodeType::IDENTIFIER; };
        bool isInAssignExpr; // whether the identifier is being referenced (ex. x + 1) or assigned (x = 1)

        // subscripts for array accessing
        void addSubscript(ASTArraySubscript* pSub) { this->subscripts.push_back(pSub); };
        const std::vector<ASTArraySubscript*>& getSubscripts() { return subscripts; };

        void determineResultType(scope_stack_t&);
        Type getResultType() const { return type; };
    private:
        Type type;
        std::vector<ASTArraySubscript*> subscripts;
};

class ASTVarDeclaration : public ASTNode {
    public:
        ASTVarDeclaration(const Token& token, Type type) : ASTNode(token), type(type) {};
        ~ASTVarDeclaration();
        ASTNodeType getNodeType() const { return ASTNodeType::VAR_DECLARATION; };
        Type getType() const { return type; }

        ASTIdentifier* pIdentifier = nullptr;
        ASTExpr* pExpr = nullptr;
        Type type; // type of variable
};

class ASTArrayLiteral : public ASTNode {
    public:
        ASTArrayLiteral(const Token& token) : ASTNode(token) {};
        ASTNodeType getNodeType() const { return ASTNodeType::LIT_ARR; };
        Type inferType(scope_stack_t&) const;

        Type getType() const { return type; };
        void setType(Type);
    private:
        Type type;
};

class ASTBoolLiteral : public ASTNode {
    public:
        ASTBoolLiteral(bool val, const Token& token) : ASTNode(token), val(val) {};
        ASTNodeType getNodeType() const { return ASTNodeType::LIT_BOOL; };
        bool val;
};

class ASTCharLiteral : public ASTNode {
    public:
        ASTCharLiteral(char val, const Token& token) : ASTNode(token), val(val) {};
        ASTNodeType getNodeType() const { return ASTNodeType::LIT_CHAR; };
        char val;
};

class ASTFloatLiteral : public ASTNode {
    public:
        ASTFloatLiteral(double val, const Token& token) : ASTNode(token), val(val) {};
        ASTNodeType getNodeType() const { return ASTNodeType::LIT_FLOAT; };
        double val;
};

class ASTIntLiteral : public ASTNode {
    public:
        ASTIntLiteral(int val, const Token& token) : ASTNode(token), val(val) {};
        ASTNodeType getNodeType() const { return ASTNodeType::LIT_INT; };
        int val;
};

class ASTVoidLiteral : public ASTNode {
    public:
        ASTVoidLiteral(const Token& token) : ASTNode(token) {};
        ASTNodeType getNodeType() const { return ASTNodeType::LIT_VOID; };
};

class ASTTypeCast : public ASTNode {
    public:
        ASTTypeCast(const Token& token, Type type) : ASTNode(token), type(type) {};
        ASTNodeType getNodeType() const { return ASTNodeType::TYPE_CAST; };
        Type type;
};

#endif