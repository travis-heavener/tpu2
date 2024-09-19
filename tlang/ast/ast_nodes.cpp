#include "ast_nodes.hpp"
#include "../util/util.hpp"

// used to help infer the type of objects
Type getTypeFromNode(ASTNode& node, scope_stack_t& scopeStack) {
    switch (node.getNodeType()) {
        case ASTNodeType::IDENTIFIER: {
            ASTIdentifier& iden = *static_cast<ASTIdentifier*>(&node);

            // lookup the variable from the scope
            Type type = lookupParserVariable(scopeStack, node.raw, node.err);

            // handle any subscripts
            size_t numSubscripts = iden.getSubscripts().size();
            for (size_t i = 0; i < numSubscripts; i++)
                type.popArrayModifier();

            return type;
        }
        case ASTNodeType::FUNCTION_CALL: return lookupParserVariable(scopeStack, node.raw, node.err);
        case ASTNodeType::LIT_INT: return Type(TokenType::TYPE_INT);
        case ASTNodeType::LIT_BOOL: return Type(TokenType::TYPE_BOOL);
        case ASTNodeType::LIT_CHAR: return Type(TokenType::TYPE_CHAR);
        case ASTNodeType::LIT_FLOAT: return Type(TokenType::TYPE_FLOAT);
        case ASTNodeType::LIT_VOID: return Type(TokenType::VOID);
        case ASTNodeType::LIT_ARR: return static_cast<ASTArrayLiteral*>(&node)->inferType(scopeStack);
        case ASTNodeType::EXPR: return static_cast<ASTExpr*>(&node)->inferType(scopeStack);
        case ASTNodeType::UNARY_OP: {
            ASTOperator* pOp = static_cast<ASTOperator*>(&node);
            switch (pOp->getOpTokenType()) {
                case TokenType::OP_ADD:
                case TokenType::OP_SUB: {
                    // verify arg is valid
                    Type typeA = getTypeFromNode(*pOp->left(), scopeStack);

                    // prevent array
                    if (typeA.isArray()) throw TInvalidOperationException(pOp->err);

                    // take size of left arg
                    return Type(typeA.getPrimitiveType());
                }
                case TokenType::OP_BIT_NOT:
                case TokenType::OP_BOOL_NOT: {
                    // verify arg is valid
                    Type typeA = getTypeFromNode(*pOp->left(), scopeStack);

                    // prevent array
                    if (typeA.isArray()) throw TInvalidOperationException(pOp->err);

                    // take size of boolean
                    return Type(TokenType::TYPE_BOOL);
                }
                default: throw TTypeInferException(pOp->err);
            }
            break;
        }
        case ASTNodeType::BIN_OP: {
            ASTOperator* pOp = static_cast<ASTOperator*>(&node);

            switch (pOp->getOpTokenType()) {
                case TokenType::OP_ADD:
                case TokenType::OP_SUB:
                case TokenType::OP_MUL:
                case TokenType::OP_DIV:
                case TokenType::OP_MOD:
                case TokenType::OP_BIT_AND:
                case TokenType::OP_BIT_OR:
                case TokenType::OP_BIT_XOR: {
                    // take size of whichever primitive is larger
                    Type typeA = getTypeFromNode(*pOp->left(), scopeStack);
                    Type typeB = getTypeFromNode(*pOp->right(), scopeStack);

                    // prevent arrays
                    if (typeA.isArray() || typeB.isArray())
                        throw TInvalidOperationException(pOp->err);
                    
                    // get larger of the two primitives
                    TokenType primA = typeA.getPrimitiveType();
                    TokenType primB = typeB.getPrimitiveType();

                    if (getSizeOfType(primA) < getSizeOfType(primB))
                        return typeB;
                    else
                        return typeA;
                }
                case TokenType::OP_LT:
                case TokenType::OP_GT:
                case TokenType::OP_LTE:
                case TokenType::OP_GTE:
                case TokenType::OP_EQ:
                case TokenType::OP_NEQ:
                case TokenType::OP_BOOL_AND:
                case TokenType::OP_BOOL_OR: {
                    // verify both args are valid
                    Type typeA = getTypeFromNode(*pOp->left(), scopeStack);
                    Type typeB = getTypeFromNode(*pOp->right(), scopeStack);

                    // prevent arrays
                    if (typeA.isArray() || typeB.isArray())
                        throw TInvalidOperationException(pOp->err);

                    // take size of boolean
                    return Type(TokenType::TYPE_BOOL);
                }
                case TokenType::OP_LSHIFT:
                case TokenType::OP_RSHIFT: {
                    Type typeA = getTypeFromNode(*pOp->left(), scopeStack);
                    Type typeB = getTypeFromNode(*pOp->right(), scopeStack);

                    // prevent arrays
                    if (typeA.isArray() || typeB.isArray())
                        throw TInvalidOperationException(pOp->err);
                    
                    // ensure right arg is int-like
                    TokenType primB = typeB.getPrimitiveType();
                    if (primB != TokenType::TYPE_INT && primB != TokenType::TYPE_CHAR && primB != TokenType::TYPE_BOOL)
                        throw TInvalidOperationException(pOp->err);

                    // take size of left arg
                    return typeA;
                }
                case TokenType::ASSIGN: {
                    // take the type of the left-arg
                    return getTypeFromNode(*pOp->left(), scopeStack);
                }
                default: throw TTypeInferException(pOp->err);
            }
            break;
        }
        default: throw TTypeInferException(node.err);
    }
}

// base destructor
ASTNode::~ASTNode() {
    for (ASTNode* pChild : this->children)
        delete pChild;
}

ASTNode* ASTNode::removeChild(size_t i) {
    ASTNode* pNode = this->children[i];
    this->children.erase(this->children.begin()+i);
    return pNode;
}

ASTIfCondition::~ASTIfCondition() {
    // free condition
    if (this->pExpr != nullptr) delete this->pExpr;
}

ASTElseIfCondition::~ASTElseIfCondition() {
    // free condition
    if (this->pExpr != nullptr) delete this->pExpr;
}

ASTForLoop::~ASTForLoop() {
    // free expressions
    if (this->pExprA != nullptr) delete this->pExprA;
    if (this->pExprB != nullptr) delete this->pExprB;
    if (this->pExprC != nullptr) delete this->pExprC;
}

ASTWhileLoop::~ASTWhileLoop() {
    // free expression
    if (this->pExpr != nullptr) delete this->pExpr;
}

ASTVarDeclaration::~ASTVarDeclaration() {
    // free identifier and expression
    if (this->pIdentifier != nullptr) delete this->pIdentifier;
    if (this->pExpr != nullptr) delete this->pExpr;
}

ASTFunction::~ASTFunction() {
    for (ASTFuncParam* p : params) {
        delete p;
    }
}

ASTIdentifier::~ASTIdentifier() {
    for (ASTArraySubscript* pSub : subscripts) {
        delete pSub;
    }
}

// determine the type of this expression
Type ASTExpr::inferType(scope_stack_t& scopeStack) const {
    Type type;
    for (ASTNode* pNode : this->children) {
        type = type.checkDominant( getTypeFromNode(*pNode, scopeStack) );
    }

    return type;
}

// get the result type of an operation
void ASTOperator::determineResultType(scope_stack_t& scopeStack) {
    // recurse over child operators
    for (ASTNode* pNode : this->children) {
        ASTNodeType nodeType = pNode->getNodeType();
        if (nodeType == ASTNodeType::BIN_OP || nodeType == ASTNodeType::UNARY_OP) {
            ASTOperator& op = *static_cast<ASTOperator*>(pNode);
            op.determineResultType(scopeStack);
        }
    }

    // update this node type
    this->returnType = getTypeFromNode(*this, scopeStack);
}

// get the result type of an identifier
void ASTIdentifier::determineResultType(scope_stack_t& scopeStack) {
    this->type = getTypeFromNode(*this, scopeStack);
}

Type ASTArrayLiteral::inferType(scope_stack_t& scopeStack) const {
    // verify type is consistant and update if necessary
    Type type;
    for (ASTNode* pNode : this->children) {
        ASTExpr* pExpr = static_cast<ASTExpr*>(pNode);
        type = type.checkDominant(pExpr->inferType(scopeStack));
    }
    type.addArrayModifier( this->size() );
    return type;
}

void ASTArrayLiteral::setType(Type type) {
    // update type
    this->type = type;

    // get the type for children (sum of children equals this)
    Type subType = type;
    subType.popArrayModifier();

    // update all children
    for (ASTNode* pNode : this->children) {
        static_cast<ASTExpr*>(pNode)->type = subType;

        // recurse for any sub-arrays
        if (pNode->at(0)->getNodeType() == ASTNodeType::LIT_ARR) {
            static_cast<ASTArrayLiteral*>(pNode->at(0))->setType(subType);
        }
    }
}