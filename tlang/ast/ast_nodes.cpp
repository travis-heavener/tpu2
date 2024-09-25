#include "ast_nodes.hpp"
#include "../util/util.hpp"

// used to help infer the type of objects
Type getTypeFromNode(ASTNode& node, scope_stack_t& scopeStack, bool overrideAsLValue=false) {
    switch (node.getNodeType()) {
        case ASTNodeType::IDENTIFIER: {
            ASTIdentifier* pIden = static_cast<ASTIdentifier*>(&node);

            // lookup the variable from the scope
            Type type = lookupParserVariable(scopeStack, node.raw, node.err);

            // handle any subscripts
            size_t numSubscripts = pIden->getSubscripts().size();
            for (size_t i = 0; i < numSubscripts; ++i) {
                if (type.getNumArrayModifiers() > 0) {
                    // pop off an array modifier if present
                    type.popArrayModifier();
                } else if (type.getNumPtrs() > 0) {
                    // if not present, pop off a pointer
                    type.popPointer();
                } else {
                    throw TSyntaxException(node.err);
                }
            }

            // if this is a pointer, store as an lvalue
            if (type.getNumPtrs() > 0 || overrideAsLValue) {
                type.setIsLValue(true);
            } else {
                type.setIsLValue(false); // force to not be an lvalue (lookupParserVariable stores lvalue status as true)
            }

            return type;
        }
        case ASTNodeType::FUNCTION_CALL: {
            Type retType = lookupParserVariable(scopeStack, node.raw, node.err);
            retType.setIsLValue(false); // prevent from being an lvalue (returned values are temporary)
            return retType;
        }
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
                case TokenType::OP_ADD: case TokenType::OP_SUB: {
                    // verify arg is valid
                    Type typeA = getTypeFromNode(*pOp->left(), scopeStack);

                    // prevent array
                    if (typeA.isArray()) throw TInvalidOperationException(pOp->err);

                    // prevent type from being an lvalue
                    if (typeA.getIsLValue()) throw TInvalidOperationException(node.err);

                    // take size of left arg
                    return typeA;
                }
                case TokenType::OP_BIT_NOT: case TokenType::OP_BOOL_NOT: {
                    // verify arg is valid
                    Type typeA = getTypeFromNode(*pOp->left(), scopeStack);

                    // prevent array
                    if (typeA.isArray()) throw TInvalidOperationException(pOp->err);

                    // prevent type from being an lvalue if this is bitwise not
                    if (pOp->getOpTokenType() == TokenType::OP_BIT_NOT) {
                        if (typeA.getIsLValue()) throw TInvalidOperationException(node.err);
                    }

                    // take size of boolean
                    return Type(TokenType::TYPE_BOOL);
                }
                case TokenType::ASTERISK: {
                    // verify dereference is valid
                    Type typeA = getTypeFromNode(*pOp->left(), scopeStack);

                    if (typeA.getNumPtrs() == 0)
                        throw TInvalidOperationException(pOp->err);
                    typeA.popPointer();

                    // force dereferenced values to be lvalues
                    typeA.setIsLValue(true);

                    return typeA;
                }
                case TokenType::AMPERSAND: {
                    // get address of lvalue
                    Type typeA = getTypeFromNode(*pOp->left(), scopeStack, true);

                    // verify the type is an lvalue
                    if (!typeA.getIsLValue()) throw TInvalidOperationException(node.err);

                    typeA.addPointer();
                    return typeA;
                }
                case TokenType::SIZEOF: {
                    // sizeof *always* returns an int
                    return Type(TokenType::TYPE_INT);
                }
                default: {
                    // handle typecast unary
                    if (pOp->getUnaryType() == ASTUnaryType::TYPE_CAST) {
                        ASTNode& typeCastChild = *static_cast<ASTTypeCast*>(pOp->left())->at(0);
                        Type retType = getTypeFromNode(typeCastChild, scopeStack);

                        // if the type is a pointer, allow it to be an lvalue
                        // for non-pointer types, disallow from being an lvalue
                        retType.setIsLValue( retType.getNumPtrs() > 0 );
                        return retType;
                    }
                    
                    // not found
                    throw TTypeInferException(pOp->err);
                }
            }
            break;
        }
        case ASTNodeType::BIN_OP: {
            ASTOperator* pOp = static_cast<ASTOperator*>(&node);

            switch (pOp->getOpTokenType()) {
                case TokenType::OP_ADD: case TokenType::OP_SUB:
                case TokenType::ASTERISK: case TokenType::OP_DIV:
                case TokenType::OP_MOD: case TokenType::AMPERSAND:
                case TokenType::OP_BIT_OR: case TokenType::OP_BIT_XOR: {
                    // take size of whichever primitive is larger
                    Type typeA = getTypeFromNode(*pOp->left(), scopeStack);
                    Type typeB = getTypeFromNode(*pOp->right(), scopeStack);

                    // prevent arrays
                    if (typeA.isArray() || typeB.isArray())
                        throw TInvalidOperationException(pOp->err);
                    
                    // get larger of the two primitives
                    TokenType primA = typeA.getPrimitiveType();
                    TokenType primB = typeB.getPrimitiveType();

                    // prevent return type from being an lvalue
                    Type retType = getSizeOfType(primA) < getSizeOfType(primB) ? typeB : typeA;
                    retType.setIsLValue(false);
                    return retType;
                }
                case TokenType::OP_LT: case TokenType::OP_GT:
                case TokenType::OP_LTE: case TokenType::OP_GTE:
                case TokenType::OP_EQ: case TokenType::OP_NEQ:
                case TokenType::OP_BOOL_AND: case TokenType::OP_BOOL_OR: {
                    // verify both args are valid
                    Type typeA = getTypeFromNode(*pOp->left(), scopeStack);
                    Type typeB = getTypeFromNode(*pOp->right(), scopeStack);

                    // prevent arrays
                    if (typeA.isArray() || typeB.isArray())
                        throw TInvalidOperationException(pOp->err);

                    // take size of boolean
                    return Type(TokenType::TYPE_BOOL);
                }
                case TokenType::OP_LSHIFT: case TokenType::OP_RSHIFT: {
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
                    Type type = getTypeFromNode(*pOp->left(), scopeStack);

                    // prevent type from being lvalue
                    type.setIsLValue(false);
                    return type;
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
    if (this->pExpr != nullptr)
        delete this->pExpr;
}

ASTVarDeclaration::~ASTVarDeclaration() {
    // free identifier and expression
    if (this->pIdentifier != nullptr) delete this->pIdentifier;
    if (this->pExpr != nullptr) delete this->pExpr;
}

ASTFunction::~ASTFunction() {
    for (ASTFuncParam* p : params)
        delete p;
}

ASTIdentifier::~ASTIdentifier() {
    for (ASTArraySubscript* pSub : subscripts)
        delete pSub;
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

    // mark any identifier as an lvalue
    this->type.setIsLValue(true);
}

Type ASTArrayLiteral::inferType(scope_stack_t& scopeStack) const {
    // verify type is consistant and update if necessary
    Type type;
    for (ASTNode* pNode : this->children) {
        ASTExpr* pExpr = static_cast<ASTExpr*>(pNode);
        type = type.checkDominant(pExpr->inferType(scopeStack));
    }
    type.addArrayModifier( this->size() );

    // prevent this from being an lvalue
    type.setIsLValue(false);
    return type;
}

void ASTArrayLiteral::setType(Type type) {
    // update type
    this->type = type;

    // prevent this from being an lvalue (children can be)
    this->type.setIsLValue(false);

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