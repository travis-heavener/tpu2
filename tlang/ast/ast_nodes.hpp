#ifndef __AST_NODES_HPP
#define __AST_NODES_HPP

#include <string>
#include <vector>

#include "../util/token.hpp"
#include "../util/scope_stack.hpp"

enum class ASTNodeType {
    NODE, // base class
    FUNCTION, FUNCTION_CALL, VAR_DECLARATION, IDENTIFIER, RETURN,
    CONDITIONAL, IF_CONDITION, ELSE_IF_CONDITION, ELSE_CONDITION,
    FOR_LOOP, WHILE_LOOP,
    EXPR, UNARY_OP, BIN_OP, TYPE_CAST, SIZEOF, ASM, ASM_INST,
    LIT_BOOL, LIT_CHAR, LIT_FLOAT, LIT_INT, LIT_VOID, LIT_STRING, LIT_ARR, ARR_SUBSCRIPT
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
        void removeByAddress(void* addr);
        ASTNode* pop() { ASTNode* pNode = lastChild(); children.pop_back(); return pNode; };
        
        virtual ASTNodeType getNodeType() const { return ASTNodeType::NODE; };

        ASTNode* at(unsigned int i) const { return children[i]; };
        void insert(ASTNode*, unsigned int);
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

// fwd dec
class ASTArraySubscript;

class ASTTypedNode : public ASTNode {
    public:
        ASTTypedNode(const Token& t) : ASTNode(t) {};
        ~ASTTypedNode();

        // infer this node's type from any children
        virtual void inferType(scope_stack_t&);
        void inferChildTypes(scope_stack_t&);
        void inferSubscriptTypes(scope_stack_t&);

        Type& getTypeRef() { return type; };
        Type getType() const { return type; };
        void setType(const Type& t) { type = t; };

        bool isLValue() const { return _isLValue; };
        void setIsLValue(bool);

        // subscripts for array accessing
        void addSubscript(ASTArraySubscript* pSub) { this->subscripts.push_back(pSub); };
        const std::vector<ASTArraySubscript*>& getSubscripts() { return subscripts; };
        size_t getNumSubscripts() const { return subscripts.size(); };
    protected:
        std::vector<ASTArraySubscript*> subscripts;
    private:
        Type type;
        bool _isLValue = false;
};

class ASTExpr : public ASTTypedNode {
    public:
        ASTExpr(const Token& token) : ASTTypedNode(token) {};
        ASTNodeType getNodeType() const { return ASTNodeType::EXPR; };
};

class ASTOperator : public ASTTypedNode {
    public:
        ASTOperator(const Token& token, bool isUnary) : ASTTypedNode(token), opType(token.type), isUnary(isUnary) {}
        ASTNodeType getNodeType() const { return isUnary ? ASTNodeType::UNARY_OP : ASTNodeType::BIN_OP; }

        ASTNode* left() { return children[0]; }
        ASTNode* right() { return children[1]; }
        TokenType getOpTokenType() const { return opType; }

        void setIsUnary(bool isUnary) { this->isUnary = isUnary; }
        bool getIsUnary() const { return this->isUnary; }

        void setUnaryType(ASTUnaryType t) { this->unaryType = t; };
        ASTUnaryType getUnaryType() const { return this->unaryType; };
        
        void inferType(scope_stack_t&);

        void setIsNullified(bool i) { _isNullified = i; };
        bool isNullified() const { return _isNullified; };
    private:
        ASTUnaryType unaryType = ASTUnaryType::BASE;
        TokenType opType;
        bool isUnary;
        bool _isNullified = false; // if this is chained to another operator and cancels out
};

class ASTInlineASM : public ASTTypedNode {
    public:
        ASTInlineASM(const Token& token, const std::string& raw) : ASTTypedNode(token), raw(raw) {};
        ASTNodeType getNodeType() const { return ASTNodeType::ASM; }
        void inferType() { this->setType( Type(TokenType::VOID) ); };

        const std::string& getRaw() const { return raw; };
    private:
        std::string raw;
};

class ASMProtectedInstruction : public ASTTypedNode {
    public:
        ASMProtectedInstruction(const Token& token) : ASTTypedNode(token), instType(token.type) {};
        ASTNodeType getNodeType() const { return ASTNodeType::ASM_INST; }
        void inferType(scope_stack_t&);
        TokenType getInstType() const { return instType; };
    private:
        const TokenType instType;
};

/************* LITERALS & IDENTIFIERS *************/

class ASTArraySubscript : public ASTTypedNode {
    public:
        ASTArraySubscript(const Token& token) : ASTTypedNode(token) {};
        ASTNodeType getNodeType() const { return ASTNodeType::ARR_SUBSCRIPT; };
        void inferType(scope_stack_t&);
};

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

        // used to load each parameter type to a vector
        void loadParamTypes(std::vector<Type>&) const;

        bool isMainFunction() const;
    private:
        std::string name; // name of function
        Type type; // return type
        std::vector<ASTFuncParam*> params; // parameters {name, type}
};

class ASTFunctionCall : public ASTTypedNode {
    public:
        ASTFunctionCall(const Token& token) : ASTTypedNode(token) {};
        ASTNodeType getNodeType() const { return ASTNodeType::FUNCTION_CALL; };
        void inferType(scope_stack_t&);
};

class ASTIdentifier : public ASTTypedNode {
    public:
        ASTIdentifier(const Token& token, bool inAssign) : ASTTypedNode(token), isInAssignExpr(inAssign) {};
        ASTNodeType getNodeType() const { return ASTNodeType::IDENTIFIER; };
        bool isInAssignExpr; // whether the identifier is being referenced (ex. x + 1) or assigned (x = 1)

        void inferType(scope_stack_t&);
};

class ASTVarDeclaration : public ASTNode {
    public:
        ASTVarDeclaration(const Token& token, Type type) : ASTNode(token), type(type) {};
        ~ASTVarDeclaration();
        ASTNodeType getNodeType() const { return ASTNodeType::VAR_DECLARATION; };
        Type getType() const { return type; }
        Type& getTypeRef() { return type; }
        void updateType(const Type&);

        ASTIdentifier* pIdentifier = nullptr;
        ASTExpr* pExpr = nullptr;
        Type type; // type of variable
};

class ASTArrayLiteral : public ASTTypedNode {
    public:
        ASTArrayLiteral(const Token& token) : ASTTypedNode(token) {};
        ASTNodeType getNodeType() const { return ASTNodeType::LIT_ARR; };

        void inferType(scope_stack_t&);
        void setTypeRecursive(const Type&);
};

class ASTBoolLiteral : public ASTTypedNode {
    public:
        ASTBoolLiteral(bool val, const Token& token) : ASTTypedNode(token), val(val) {};
        ASTNodeType getNodeType() const { return ASTNodeType::LIT_BOOL; };
        void inferType(scope_stack_t&);
        bool val;
};

class ASTCharLiteral : public ASTTypedNode {
    public:
        ASTCharLiteral(short val, const Token& token) : ASTTypedNode(token), val(val) {};
        ASTNodeType getNodeType() const { return ASTNodeType::LIT_CHAR; };
        void inferType(scope_stack_t&);
        short val;
};

class ASTFloatLiteral : public ASTTypedNode {
    public:
        ASTFloatLiteral(double val, const Token& token) : ASTTypedNode(token), val(val) {};
        ASTNodeType getNodeType() const { return ASTNodeType::LIT_FLOAT; };
        void inferType(scope_stack_t&);
        double val;
};

class ASTIntLiteral : public ASTTypedNode {
    public:
        ASTIntLiteral(int val, const Token& token) : ASTTypedNode(token), val(val) {};
        ASTNodeType getNodeType() const { return ASTNodeType::LIT_INT; };
        void inferType(scope_stack_t&);
        int val;
};

class ASTVoidLiteral : public ASTTypedNode {
    public:
        ASTVoidLiteral(const Token& token) : ASTTypedNode(token) {};
        ASTNodeType getNodeType() const { return ASTNodeType::LIT_VOID; };
        void inferType(scope_stack_t&);
};

class ASTStringLiteral : public ASTTypedNode {
    public:
        ASTStringLiteral(const Token& token, const std::string& str) : ASTTypedNode(token), str(str), token(token) {};
        ASTNodeType getNodeType() const { return ASTNodeType::LIT_STRING; };
        void inferType(scope_stack_t&);
        ASTNode* asCharArr() const;
        const std::string str;
    private:
        Token token;
};

class ASTTypeCast : public ASTTypedNode {
    public:
        ASTTypeCast(const Token& token) : ASTTypedNode(token), token(token) {};
        ASTNodeType getNodeType() const { return ASTNodeType::TYPE_CAST; };

        ASTOperator* toOperator(ASTNode*);
    private:
        Token token;
};

#endif