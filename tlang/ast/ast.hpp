#ifndef __AST_HPP
#define __AST_HPP

#include <vector>

#include "ast_nodes.hpp"

/**
 * A great deal of files in this ast directory have been graciously lifted from another project of mine
 * called "Deuterium," an (at the time of writing this) unfinished, compiled programming language similar to C.
 * 
 * https://github.com/travis-heavener/deuterium
 */

class AST {
    public:
        ~AST() {  for (ASTNode* pNode : children) delete pNode;  }
        void push(ASTNode* pNode) {  children.push_back(pNode);  }
        std::vector<ASTNode*>& getChildren() {  return children;  }
        size_t size() const { return children.size(); }
        void removeByAddress(void*);
    private:
        std::vector<ASTNode*> children;
};

#endif