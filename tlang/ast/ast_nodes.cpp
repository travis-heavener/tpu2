#include "ast_nodes.hpp"
#include "../util/config.hpp"
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

void ASTNode::removeByAddress(void* addr) {
    for (size_t i = 0; i < children.size(); ++i) {
        if (children[i] == addr) {
            delete removeChild(i);
            return;
        }
    }
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

// returns true if this ASTFunction the main function
bool ASTFunction::isMainFunction() const {
    return name == FUNC_MAIN_NAME && type == Type(TokenType::TYPE_INT) && params.size() == 0;
}

ASTNode* ASTStringLiteral::asCharArr() const {
    // transform a string literal into a character array literal
    ASTArrayLiteral* pArr = new ASTArrayLiteral(token);
    std::string rawString = raw.substr(1, raw.size()-2) + '\0'; // add null byte and remove quotes
    escapeString(rawString); // escape the string

    // add each character as a child
    Type type(TokenType::TYPE_CHAR);
    for (const char c : rawString) {
        // create wrapper expression
        ASTExpr* pSubExpr = new ASTExpr(token);
        ASTCharLiteral* pChar = new ASTCharLiteral(c, token);
        pArr->push(pSubExpr);
        pSubExpr->push(pChar);

        // imply type of child
        pChar->setType(type);
        pSubExpr->setType(type);
    }

    // set type
    type.addHintPointer(pArr->size());
    pArr->setType(type);

    return pArr;
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

    // append subscripts
    for (ASTArraySubscript* pSub : subscripts)
        pOp->addSubscript(pSub);
    
    // append children (shouldn't be used but just in case)
    for (ASTNode* pChild : children)
        pOp->push(pChild);
    return pOp;
}

void ASTVarDeclaration::updateType(const Type& type) {
    // update own type
    this->type = type;

    // update childrens' types
    this->pIdentifier->setType(type);
    this->pExpr->setType(type);

    // enforce array types
    if (pExpr->at(0)->getNodeType() == ASTNodeType::LIT_ARR) {
        ASTArrayLiteral* pArrLit = static_cast<ASTArrayLiteral*>(pExpr->at(0));
        pArrLit->setTypeRecursive(type);
    } else if (pExpr->at(0)->getNodeType() == ASTNodeType::LIT_STRING) {
        // determine if this will be stack'd or not
        if (type.getNumArrayHints() == 1) {
            // convert to char array
            ASTStringLiteral* pChild = static_cast<ASTStringLiteral*>(pExpr->at(0));
            pExpr->removeChild(0);
            pExpr->push( pChild->asCharArr() );
            delete pChild;

            // update this type
            this->type = pExpr->getTypeRef();
            this->pIdentifier->setType( pExpr->getTypeRef() );
        }
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

    for (size_t i = 0; i < children.size(); ++i) {
        ASTNode* pChild = children[i];

        // update child if child is array
        if (pChild->getNodeType() == ASTNodeType::LIT_ARR) {
            // sets the child's type to what we want, so don't need to check it
            static_cast<ASTArrayLiteral*>(pChild)->setTypeRecursive(desiredChildType);
        } else if (pChild->getNodeType() == ASTNodeType::LIT_STRING) {
            if (desiredChildType.getNumArrayHints() == 1) { // force on stack
                // convert to char array
                removeChild(i);
                insert(static_cast<ASTStringLiteral*>(pChild)->asCharArr(), i);
                delete pChild;
            } else if (desiredChildType.getNumPointers() != 1) { // if char pointer, keep as one
                throw TSyntaxException(err);
            }
        } else {
            // number of children already confirmed to match, so update type
            static_cast<ASTTypedNode*>(pChild)->setType(desiredChildType);
        }
    }
}

// used to load each parameter type to a vector
void ASTFunction::loadParamTypes(std::vector<Type>& paramTypes) const {
    for (ASTFuncParam* pParam : params) {
        paramTypes.push_back(pParam->type);
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

    // infer subscripts
    this->inferSubscriptTypes(scopeStack);
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

            // copy subscripts to last child
            ASTTypedNode* pTypedNode = static_cast<ASTTypedNode*>(pNode);
            ASTTypedNode* pLastChild = static_cast<ASTTypedNode*>(this->lastChild());
            while (pTypedNode->getNumSubscripts() > 0) {
                pLastChild->addSubscript( pTypedNode->subscripts[0] );
                pTypedNode->subscripts.erase(pTypedNode->subscripts.begin()+0);
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

void ASTTypedNode::inferSubscriptTypes(scope_stack_t& scopeStack) {
    // infer subscripts' types
    if (this->subscripts.size() == 0) return;

    if (!this->type.isPointer())
        throw TInvalidOperationException(this->subscripts[0]->err);

    for (ASTArraySubscript* pSub : subscripts) {
        pSub->inferType(scopeStack);
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

                // nullify chained operations that cancel
                ASTOperator* pAOp = dynamic_cast<ASTOperator*>(pA);
                if (pAOp != nullptr && !pAOp->_isNullified && pAOp->opType == opType) {
                    this->setIsNullified(true);
                    pAOp->setIsNullified(true);
                }
                break;
            }
            case TokenType::OP_BOOL_NOT: {
                if (typeA.isVoidNonPtr()) // verify non-void
                    throw TInvalidOperationException(err);
                this->setType( Type(TokenType::TYPE_BOOL) );
                pA->setIsLValue(false); // revoke lvalue status

                // if pA is also OP_BOOL_NOT, nullify
                ASTOperator* pAOp = dynamic_cast<ASTOperator*>(pA);
                if (pAOp != nullptr && !pAOp->_isNullified && pAOp->opType == opType) {
                    this->setIsNullified(true);
                    pAOp->setIsNullified(true);
                }
                break;
            }
            case TokenType::ASTERISK: {
                /**
                 * child:   lvalue/rvalue (pointer)
                 * result:  rvalue (can be lvalue)
                 */
                if (typeA.getNumPointers() == 0 || typeA.isVoidPtr()) // check for pointers
                    throw TInvalidOperationException(err);

                this->setType(typeA);
                this->getTypeRef().popPointer();
                pA->setIsLValue(false); // revoke lvalue status
                break;
            }
            case TokenType::AMPERSAND: {
                /**
                 * child:   lvalue
                 * result:  rvalue
                 */
                // set left arg to lvalue if identifier
                if (pA->getNodeType() == ASTNodeType::IDENTIFIER)
                    pA->setIsLValue(true);
                
                // if dereferenced value, set as lvalue
                if (pA->getNodeType() == ASTNodeType::UNARY_OP &&
                    static_cast<ASTOperator*>(pA)->getOpTokenType() == TokenType::ASTERISK)
                    pA->setIsLValue(true);

                // verify lvalue
                if (!pA->isLValue()) throw TInvalidOperationException(err);

                this->setType( typeA );
                this->getTypeRef().addEmptyPointer();

                // if getting the address of something dereferenced, nullify this and pA
                if (pA->getNodeType() == ASTNodeType::UNARY_OP) {
                    ASTOperator* pAOp = static_cast<ASTOperator*>(pA);
                    if (pAOp->getOpTokenType() == TokenType::ASTERISK) {
                        pAOp->setIsNullified(true);
                        this->setIsNullified(true);
                    }
                }
                break;
            }
            case TokenType::SIZEOF: {
                // verify non-void
                if (typeA.isVoidNonPtr()) throw TInvalidOperationException(err);
                this->setType( Type(TokenType::TYPE_INT) );
                pA->setIsLValue(true);
                break;
            }
            default: {
                // handle typecast unary
                if (unaryType == ASTUnaryType::TYPE_CAST) {
                    // verify not typecasting a void
                    if (typeA.isVoidNonPtr())
                        throw TInvalidOperationException(err);

                    // if this is an array, the desired type is now a pointer TO the array
                    if (typeA.isArray()) {
                        pA->setIsLValue(true); // force lvalue
                        typeA.addEmptyPointer();
                        pA->setType(typeA);
                    } else {
                        pA->setIsLValue(false); // revoke lvalue status from child
                    }
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
                } else { // one is a pointer
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
                if (typeB.isPointer() || typeB.getPrimType() != TokenType::TYPE_INT)
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

                // if dereferenced value, set as lvalue
                if (pA->getNodeType() == ASTNodeType::UNARY_OP &&
                    static_cast<ASTOperator*>(pA)->getOpTokenType() == TokenType::ASTERISK)
                    pA->setIsLValue(true);

                // verify left arg is lvalue and isn't still an array (is fully subscripted/dereferenced)
                if (!pA->isLValue() || typeA.isArray()) throw TInvalidOperationException(err);

                // verify right arg is not void
                if (typeB.isVoidNonPtr()) throw TIllegalVoidUseException(err);

                // verify not assigning to a const type
                if (typeA.isConst()) throw TConstAssignmentException(err);

                this->setType( typeA ); // take type of left argument
                pB->setIsLValue(false); // revoke lvalue status from child
                pB->setType( typeA ); // force left value to take type
                break;
            }
            default: throw TTypeInferException(err);
        }
    }

    // infer subscripts
    ASTTypedNode::inferSubscriptTypes(scopeStack);
}

void ASTFunctionCall::inferType(scope_stack_t& scopeStack) {
    // infer type of children (arguments/parameters)
    ASTTypedNode::inferChildTypes(scopeStack);

    // get this node's return type from scope
    std::vector<Type> paramTypes;
    for (ASTNode* pChild : children)
        paramTypes.push_back(static_cast<ASTTypedNode*>(pChild)->getTypeRef());

    int status;
    ParserFunction* pParserFunc = lookupParserFunction(scopeStack, this->raw, this->err, paramTypes, status);

    // if this is a partial match, update own types
    if (status == TYPE_PARAM_IMPLICIT_MATCH) {
        size_t i = 0;
        for (const Type& paramType : pParserFunc->getParamTypes()) {
            static_cast<ASTTypedNode*>(children[i++])->setType(paramType);
        }
    }

    // set own type to return type
    this->setType( pParserFunc->type );

    // infer subscripts
    this->inferSubscriptTypes(scopeStack);
}

void ASTArraySubscript::inferType(scope_stack_t& scopeStack) {
    // infer type of children
    ASTTypedNode::inferChildTypes(scopeStack);

    // set own type to int
    this->setType( Type(TokenType::TYPE_INT) );

    // infer subscripts
    this->inferSubscriptTypes(scopeStack);
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

    // infer subscripts
    this->inferSubscriptTypes(scopeStack);
}

void ASMProtectedInstruction::inferType(scope_stack_t& scopeStack) {
    // infer childrens' types
    this->inferChildTypes(scopeStack);

    // set own type
    const TokenType instType = this->getInstType();
    if (instType == TokenType::ASM_LOAD_AX || instType == TokenType::ASM_LOAD_BX || instType == TokenType::ASM_LOAD_CX || instType == TokenType::ASM_LOAD_DX) {
        this->setType( Type(TokenType::VOID) );
    } else if (instType == TokenType::ASM_READ_AX || instType == TokenType::ASM_READ_BX || instType == TokenType::ASM_READ_CX || instType == TokenType::ASM_READ_DX) {
        this->setType( Type(TokenType::TYPE_INT, true) );
    } else {
        throw TSyntaxException(err);
    }
}

/******** NODES BELOW DON'T HAVE CHILDREN ********/

void ASTIdentifier::inferType(scope_stack_t& scopeStack) {
    // lookup the variable from the scope
    Type type = lookupParserVariable(scopeStack, this->raw, this->err)->type;

    // infer subscripts
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

    // if this is an array and isn't fully dereferenced, force it as a pointer
    if (type.isArray() && numSubscripts < type.getNumPointers())
        this->setIsLValue(true);

    // set identifier's type
    this->setType( type );
}

void ASTIntLiteral::inferType(scope_stack_t&) {  this->setType(Type(TokenType::TYPE_INT));  }
void ASTCharLiteral::inferType(scope_stack_t&) {  this->setType(Type(TokenType::TYPE_CHAR));  }
void ASTFloatLiteral::inferType(scope_stack_t&) {  this->setType(Type(TokenType::TYPE_FLOAT));  }
void ASTBoolLiteral::inferType(scope_stack_t&) {  this->setType(Type(TokenType::TYPE_BOOL));  }
void ASTVoidLiteral::inferType(scope_stack_t&) {  this->setType(Type(TokenType::VOID));  }

void ASTStringLiteral::inferType(scope_stack_t&) {
    // by default set as const char*
    this->setType( Type(TokenType::TYPE_CHAR) );
    this->getTypeRef().addEmptyPointer();
    this->getTypeRef().setIsConst(true);
}

/************************ END TYPE INFERS ************************/