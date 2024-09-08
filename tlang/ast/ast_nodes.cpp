#include "ast_nodes.hpp"

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
    delete this->pExpr;

    // free children
    for (ASTNode* pChild : this->children)
        delete pChild;
}

ASTElseIfCondition::~ASTElseIfCondition() {
    // free condition
    delete this->pExpr;

    // free children
    for (ASTNode* pChild : this->children)
        delete pChild;
}

ASTForLoop::~ASTForLoop() {
    // free expressions
    delete this->pExprA;
    delete this->pExprB;
    delete this->pExprC;

    // free children
    for (ASTNode* pChild : this->children)
        delete pChild;
}

ASTWhileLoop::~ASTWhileLoop() {
    // free expression
    delete this->pExpr;

    // free children
    for (ASTNode* pChild : this->children)
        delete pChild;
}