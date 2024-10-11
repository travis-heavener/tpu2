#include "ast.hpp"

void AST::removeByAddress(void* addr) {
    for (size_t i = 0; i < children.size(); ++i) {
        if (children[i] == addr) {
            children.erase(children.begin() + i);
            delete (ASTNode*)addr;
            return;
        }
    }
}