#include "ast_nodes.hpp"
#include "../util/toolbox.hpp"
#include "../util/token.hpp"
#include "../util/t_exception.hpp"

void ASTNode::insert(ASTNode* pNode, unsigned int i) {
    this->children.insert(children.begin() + i, pNode);
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

ASTTypedNode::~ASTTypedNode() {
    for (ASTNode* pSub : subscripts)
        delete pSub;
}

ASTOperator* ASTTypeCast::toOperator(ASTNode* pChild) {
    // turn this node into an operator
    ASTOperator* pOp = new ASTOperator(token, true);
    pOp->push(pChild);
    pOp->setUnaryType( ASTUnaryType::TYPE_CAST );
    pOp->setType(this->getTypeRef());
    return pOp;
}

void ASTVarDeclaration::updateType(const Type& type) {
    // update own type
    this->type = type;

    // update childrens' types
    this->pIdentifier->setType(type);
    this->pExpr->setType(type);

    // enforce array types
    if (this->pExpr->at(0)->getNodeType() == ASTNodeType::LIT_ARR) {
        ASTArrayLiteral* pArrLit = static_cast<ASTArrayLiteral*>(pExpr->at(0));
        pArrLit->setTypeRecursive(type);
    }
}

void ASTArrayLiteral::setTypeRecursive(const Type& type) {
    // set own type
    this->setType(type);

    // verify number of children matches type
    const size_t numHints = type.getNumArrayHints();
    if (numHints == 0) throw TIllegalArraySizeException(err);
    if (type.getArrayHint(numHints-1) != this->size())
        throw TIllegalArraySizeException(err);

    // recurse over children
    Type desiredChildType = type;
    desiredChildType.popPointer();

    for (ASTNode* pChild : this->children) {
        // update child if child is array
        if (pChild->getNodeType() == ASTNodeType::LIT_ARR) {
            // sets the child's type to what we want, so don't need to check it
            static_cast<ASTArrayLiteral*>(pChild)->setTypeRecursive(desiredChildType);
        } else {
            // number of children already confirmed to match, so update type
            static_cast<ASTTypedNode*>(pChild)->setType(desiredChildType);
        }
    }
}

/************************ START TYPE INFERS ************************/

// base inferType method, just checks each child and finds the dominant type
void ASTTypedNode::inferType(scope_stack_t& scopeStack) {
    // infer childrens' types
    this->inferChildTypes(scopeStack);

    // infer own type from child
    for (ASTNode* pNode : this->children) {
        ASTTypedNode* pChild = static_cast<ASTTypedNode*>(pNode);
        this->type = getDominantType( this->type, pChild->type );
    }
}

// used to infer each child's type
void ASTTypedNode::inferChildTypes(scope_stack_t& scopeStack) {
    for (size_t i = 0; i < this->children.size(); ++i) {
        ASTNode* pNode = this->children[i];

        // if this is an expr, remove the wrapper
        if (pNode->getNodeType() == ASTNodeType::EXPR) {
            this->removeChild(i);

            while (pNode->size() > 0) {
                this->push( pNode->at(0) );
                pNode->removeChild(0);
            }

            delete pNode;
            --i;
        } else {
            // infer type of child
            ASTTypedNode* pChild = static_cast<ASTTypedNode*>(pNode);
            pChild->inferType(scopeStack);
        }
    }
}

// sets the value to be an lvalue IF it is a valid lvalue itself
void ASTTypedNode::setIsLValue(bool isLValue) {
    this->_isLValue = isLValue;
}

// get the result type of an operation
void ASTOperator::inferType(scope_stack_t& scopeStack) {
    // infer the type of all children
    ASTTypedNode::inferChildTypes(scopeStack);

    // compare each operand
    if (this->isUnary) {
        // get unary operand
        // NOTE: for typecasting, the actual argument is child #1, but the desired type is from child #0 (ASTTypeCast)
        // so this works
        ASTTypedNode* pA = static_cast<ASTTypedNode*>(children[0]);
        Type typeA = pA->getTypeRef();

        // handle each operation
        switch (opType) {
            case TokenType::OP_ADD: case TokenType::OP_SUB: case TokenType::OP_BIT_NOT: {
                if (typeA.isPointer() || typeA.isVoidNonPtr()) // verify non-void & non-pointer
                    throw TInvalidOperationException(err);
                this->setType(typeA);
                pA->setIsLValue(false); // revoke lvalue status
                break;
            }
            case TokenType::OP_BOOL_NOT: {
                if (typeA.isVoidNonPtr()) // verify non-void
                    throw TInvalidOperationException(err);
                this->setType( Type(TokenType::TYPE_BOOL) );
                pA->setIsLValue(false); // revoke lvalue status
                break;
            }
            case TokenType::ASTERISK: {
                if (typeA.getNumPointers() == 0) // check for pointers
                    throw TInvalidOperationException(err);

                // if dereferencing an array, force as a pointer
                if (typeA.isArray())
                    pA->getTypeRef().setForcedPointer(true);

                typeA.popPointer(); // pop a pointer off and set type
                if (typeA.isArray()) // recheck forced ptr status after popping a ptr
                    typeA.setForcedPointer(true);

                if (typeA.isVoidNonPtr()) // check for dereferenced void pointer
                    throw TInvalidOperationException(err);

                this->setType( typeA );
                pA->setIsLValue(false); // revoke lvalue status from child
                this->setIsLValue(true); // force lvalue status on this (NOT CHILD)
                break;
            }
            case TokenType::AMPERSAND: {
                // set left arg to lvalue if identifier
                if (pA->getNodeType() == ASTNodeType::IDENTIFIER)
                    pA->setIsLValue(true);

                // verify lvalue
                if (!pA->isLValue()) throw TInvalidOperationException(err);

                this->setType( typeA );
                this->getTypeRef().addEmptyPointer();

                // tell child to be just its pointer without any array hints (to fix its size to that of a ptr)
                typeA.clearArrayHints();
                typeA.setForcedPointer(true); // prevent derefs from pushing values
                pA->setType( typeA );
                break;
            }
            case TokenType::SIZEOF: {
                // verify non-void
                if (typeA.isVoidNonPtr()) throw TInvalidOperationException(err);
                this->setType( Type(TokenType::TYPE_INT) );
                pA->setIsLValue(false);
                break;
            }
            default: {
                // handle typecast unary
                if (unaryType == ASTUnaryType::TYPE_CAST) {
                    // verify not typecasting a void
                    if (typeA.isVoidNonPtr())
                        throw TInvalidOperationException(err);

                    pA->setIsLValue(false); // revoke lvalue status
                    break;
                }

                // invalid operator
                throw TTypeInferException(err);
            }
        }
    } else {
        ASTTypedNode* pA = static_cast<ASTTypedNode*>(children[0]);
        ASTTypedNode* pB = static_cast<ASTTypedNode*>(children[1]);
        Type typeA = pA->getTypeRef();
        Type typeB = pB->getTypeRef();

        // handle each operation
        switch (opType) {
            case TokenType::OP_SUB: {
                // verify non-void (including pointers)
                if (typeA.isVoidAny() || typeB.isVoidAny())
                    throw TInvalidOperationException(err);

                // allow both to be pointers
                if (typeA.isPointer() && typeB.isPointer()) {
                    // set both children to be MEM_ADDR_TYPE
                    pA->setType( MEM_ADDR_TYPE );
                    pB->setType( MEM_ADDR_TYPE );

                    // result type is just a memory address
                    this->setType( MEM_ADDR_TYPE );
                } else if (!typeA.isPointer() && !typeB.isPointer()) {
                    // both are not pointers, dominant type will take care of that so set the pointer to a mem addr
                    if (typeA.isPointer()) pA->setType( MEM_ADDR_TYPE );
                    if (typeB.isPointer()) pB->setType( MEM_ADDR_TYPE );

                    // assume dominant type
                    this->setType( getDominantType(typeA, typeB) );
                } else {
                    // one is a pointer
                    if (typeA.isArray()) {
                        typeA.setForcedPointer(true);
                        pA->getTypeRef().setForcedPointer(true);
                    } else if (typeB.isArray()) {
                        typeB.setForcedPointer(true);
                        pB->getTypeRef().setForcedPointer(true);
                    }

                    // set type to pointer
                    this->setType( typeA.isPointer() ? typeA : typeB );
                }

                // revoke lvalue status from children
                pA->setIsLValue(false);
                pB->setIsLValue(false);
                break;
            }
            case TokenType::OP_ADD:
            case TokenType::ASTERISK: case TokenType::OP_DIV:
            case TokenType::OP_MOD: case TokenType::AMPERSAND:
            case TokenType::OP_BIT_OR: case TokenType::OP_BIT_XOR: {
                // prevent both from being pointers
                if (typeA.isPointer() && typeB.isPointer())
                    throw TInvalidOperationException(err);

                // verify non-void (including void pointers)
                if (typeA.isVoidAny() || typeB.isVoidAny())
                    throw TInvalidOperationException(err);

                // verify not a pointer unless addition
                if ((typeA.isPointer() || typeB.isPointer()) && opType != TokenType::OP_ADD)
                    throw TInvalidOperationException(err);

                if (typeA.isPointer() || typeB.isPointer()) {
                    if (typeA.isArray()) {
                        typeA.setForcedPointer(true);
                        pA->getTypeRef().setForcedPointer(true);
                    } else if (typeB.isArray()) {
                        typeB.setForcedPointer(true);
                        pB->getTypeRef().setForcedPointer(true);
                    }

                    // set type to pointer
                    this->setType( typeA.isPointer() ? typeA : typeB );
                } else {
                    // assume dominant type
                    this->setType( getDominantType(typeA, typeB) );
                }

                // revoke lvalue status from children
                pA->setIsLValue(false);
                pB->setIsLValue(false);
                break;
            }
            case TokenType::OP_LT: case TokenType::OP_GT:
            case TokenType::OP_LTE: case TokenType::OP_GTE:
            case TokenType::OP_EQ: case TokenType::OP_NEQ:
            case TokenType::OP_BOOL_AND: case TokenType::OP_BOOL_OR: {
                // pointers are ALLOWED here, just not void still
                if (typeA.isVoidNonPtr() || typeB.isVoidNonPtr())
                    throw TInvalidOperationException(err);

                // turn any pointers into pure memory addresses
                if (typeA.isPointer()) pA->setType( MEM_ADDR_TYPE );
                if (typeB.isPointer()) pB->setType( MEM_ADDR_TYPE );

                // take size of boolean
                this->setType( Type(TokenType::TYPE_BOOL) );

                // revoke lvalue status from children
                pA->setIsLValue(false);
                pB->setIsLValue(false);
                break;
            }
            case TokenType::OP_LSHIFT: case TokenType::OP_RSHIFT: {
                // verify right argument is an integer and not a pointer
                if (typeB.isPointer() || typeB.getPrimitiveType() != TokenType::TYPE_INT)
                    throw TInvalidOperationException(err);

                // prevent voids or pointers
                if (typeA.isVoidNonPtr() || typeA.isPointer())
                    throw TInvalidOperationException(err);

                // take left type
                this->setType( typeA );
                
                // revoke lvalue status from children
                pA->setIsLValue(false);
                pB->setIsLValue(false);
                break;
            }
            case TokenType::ASSIGN: {
                // set left arg to lvalue if identifier
                if (pA->getNodeType() == ASTNodeType::IDENTIFIER)
                    pA->setIsLValue(true);

                // verify left arg is lvalue
                if (!pA->isLValue()) throw TInvalidOperationException(err);

                // verify right arg is not void
                if (typeB.isVoidNonPtr()) throw TIllegalVoidUseException(err);

                // assume type of left argument
                this->setType( typeA );
                pB->setIsLValue(false); // revoke lvalue status from child
                break;
            }
            default: throw TTypeInferException(err);
        }
    }
}

void ASTFunctionCall::inferType(scope_stack_t& scopeStack) {
    // infer type of children (arguments/parameters)
    ASTTypedNode::inferChildTypes(scopeStack);

    // get this node's return type from scope
    this->setType( lookupParserVariable(scopeStack, this->raw, this->err) );
}

void ASTArraySubscript::inferType(scope_stack_t& scopeStack) {
    // infer type of children
    ASTTypedNode::inferChildTypes(scopeStack);

    // set own type to int
    this->setType( Type(TokenType::TYPE_INT) );
}

void ASTArrayLiteral::inferType(scope_stack_t& scopeStack) {
    // infer childrens' types
    this->inferChildTypes(scopeStack);

    // infer own type from child
    for (ASTNode* pNode : this->children) {
        ASTTypedNode* pChild = static_cast<ASTTypedNode*>(pNode);
        this->setType( getDominantType( this->getTypeRef(), pChild->getTypeRef() ) );
    }

    // add pointer since this is an array
    this->getTypeRef().addHintPointer( this->children.size() );
}

/******** NODES BELOW DON'T HAVE CHILDREN ********/

void ASTIdentifier::inferType(scope_stack_t& scopeStack) {
    // lookup the variable from the scope
    Type type = lookupParserVariable(scopeStack, this->raw, this->err);

    // handle any subscripts
    size_t numSubscripts = this->subscripts.size();
    for (size_t i = 0; i < numSubscripts; ++i) {
        // infer the subscript's type
        this->subscripts[i]->inferType(scopeStack);

        // strip pointers from type
        if (type.getNumPointers() > 0)
            type.popPointer();
        else
            throw TSyntaxException(this->err);
    }

    // set identifier's type
    this->setType( type );
}

void ASTIntLiteral::inferType(scope_stack_t&) {  this->setType(Type(TokenType::TYPE_INT));  }
void ASTCharLiteral::inferType(scope_stack_t&) {  this->setType(Type(TokenType::TYPE_CHAR));  }
void ASTFloatLiteral::inferType(scope_stack_t&) {  this->setType(Type(TokenType::TYPE_FLOAT));  }
void ASTBoolLiteral::inferType(scope_stack_t&) {  this->setType(Type(TokenType::TYPE_BOOL));  }
void ASTVoidLiteral::inferType(scope_stack_t&) {  this->setType(Type(TokenType::VOID));  }

/************************ END TYPE INFERS ************************/