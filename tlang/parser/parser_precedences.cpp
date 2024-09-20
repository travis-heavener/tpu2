#include <stack>
#include <vector>

#include "parser_precedences.hpp"
#include "parser.hpp"

#include "../util/util.hpp"
#include "../ast/ast_nodes.hpp"

void parsePrecedence1(const std::vector<Token>& tokens, size_t startIndex, size_t endIndex, ASTNode* pHead, scope_stack_t& scopeStack) {
    // start by parsing all tokens directly (parsing sub expressions recursively)
    // precedence 1 (L -> R)
    for (size_t i = startIndex; i <= endIndex; i++) {
        if (tokens[i].type == TokenType::LPAREN) {
            // find closing parenthesis
            if (i+1 > endIndex) throw TUnclosedGroupException(tokens[i].err);

            size_t start = i, parensOpen = 1;
            do {
                parensOpen += (tokens[++i].type == TokenType::RPAREN) ? -1 :
                            (tokens[i].type == TokenType::LPAREN) ? 1 : 0;
            } while (i <= endIndex && parensOpen > 0);

            if (i > endIndex) throw TUnclosedGroupException(tokens[start].err);

            // check for function call
            if (start - 1 >= startIndex && tokens[start-1].type == TokenType::IDENTIFIER) {
                // create function call node
                ASTFunctionCall* pCall = new ASTFunctionCall(tokens[start-1]);
                pHead->push( pCall ); // append function call node

                // append sub expressions separated by a comma
                size_t end = i; // ends ON the closing parenthesis
                size_t subExprStart = start+1;
                std::stack<size_t> parensIndices; // only break subexprs that aren't inside parentheticals
                for (size_t j = start+1; j < end; j++) {
                    if (tokens[j].type == TokenType::LPAREN) {
                        parensIndices.push(j);
                    } else if (tokens[j].type == TokenType::RPAREN) {
                        parensIndices.pop();
                    } else if (parensIndices.size() == 0 && tokens[j].type == TokenType::COMMA) { // append subexpr
                        pCall->push( parseExpression(tokens, subExprStart, j-1, scopeStack) );
                        subExprStart = j+1;
                    } else if (parensIndices.size() == 0 && j+1 == end) { // append subexpr
                        pCall->push( parseExpression(tokens, subExprStart, j, scopeStack) );
                    }
                }

                // verify parenthesis are closed appropriately
                if (parensIndices.size() > 0)
                    throw TUnclosedGroupException(tokens[parensIndices.top()].err);
            } else if (start + 1 <= endIndex && isTokenPrimitiveType(tokens[start+1].type)) { // check for typecast
                // grab type (only allow pointer asterisk or typename)
                Type type( tokens[start+1].type );

                for (size_t j = start+2; j < i; ++j) {
                    // only accept asterisk
                    if (tokens[j].type != TokenType::ASTERISK)
                        throw TInvalidTokenException(tokens[j].err);
                    
                    // add to type
                    type.addPointer();
                }

                // append typecast as unary
                ASTOperator* pOp = new ASTOperator(tokens[start], true);
                pOp->setUnaryType(ASTUnaryType::TYPE_CAST);
                pOp->push( new ASTTypeCast(tokens[start], type) );
                pHead->push( pOp );
            } else { // found a subexpression
                pHead->push( parseExpression(tokens, start+1, i-1, scopeStack) ); // recurse
            }
        } else if (tokens[i].type == TokenType::LIT_INT) {
            pHead->push( new ASTIntLiteral(std::stoi(tokens[i].raw), tokens[i]) );
        } else if (tokens[i].type == TokenType::LIT_FLOAT) {
            pHead->push( new ASTFloatLiteral(std::stod(tokens[i].raw), tokens[i]) );
        } else if (tokens[i].type == TokenType::LIT_CHAR) {
            std::string str = tokens[i].raw.substr(1); // remove leading quote
            str.pop_back(); // remove trailing quote

            // prevent char literals that are more than one char
            if ((str.size() > 1 && str[0] != '\\') || (str.size() > 2))
                throw TInvalidTokenException(tokens[i].err);

            char c = str[0] == '\\' ? escapeChar( str ) : str[0]; // escape if needed
            pHead->push( new ASTCharLiteral(c, tokens[i]) );
        } else if (tokens[i].type == TokenType::LIT_BOOL) {
            pHead->push( new ASTBoolLiteral(tokens[i].raw == "true", tokens[i]) );
        } else if (tokens[i].type == TokenType::LIT_STRING) {
            std::string rawString = tokens[i].raw.substr(1, tokens[i].raw.size()-2);
            escapeString( rawString );
            rawString.push_back('\0'); // add null byte

            // transform a string literal into a character array literal
            ASTArrayLiteral* pArr = new ASTArrayLiteral(tokens[i]);
            pHead->push(pArr);

            // add each character as a child
            Type type(TokenType::TYPE_CHAR);
            for (const char c : rawString) {
                // create wrapper expression
                ASTExpr* pSubExpr = new ASTExpr(tokens[i]);
                pArr->push(pSubExpr);
                pSubExpr->push( new ASTCharLiteral(c, tokens[i]) );
                pSubExpr->type = type;
            }

            // set type
            type.addArrayModifier(pArr->size());
            pArr->setType( type );
        } else if (tokens[i].type == TokenType::VOID) {
            pHead->push( new ASTVoidLiteral(tokens[i]) );
        } else if (isTokenUnaryOp(tokens[i].type) || isTokenBinaryOp(tokens[i].type)) { // handle operators
            pHead->push( new ASTOperator(tokens[i], isTokenUnaryOp(tokens[i].type)) );
        } else if (tokens[i].type == TokenType::IDENTIFIER) {
            // skip if this is actually a function call
            if (i+1 <= endIndex && tokens[i+1].type == TokenType::LPAREN) continue;

            // if there's a next token and it's an assignment operator
            bool isAssignExpr = i+1 <= endIndex && isTokenAssignOp(tokens[i+1].type);
            pHead->push( new ASTIdentifier(tokens[i], isAssignExpr) );
        } else if (tokens[i].type == TokenType::LBRACE) { // parse array literal
            ASTArrayLiteral* pArr = new ASTArrayLiteral(tokens[i]);
            pHead->push(pArr);

            // find RBRACE
            std::vector<size_t> groupsOpen = {i++};
            size_t exprStart = i;
            while (i <= endIndex && groupsOpen.size() > 0) {
                if (tokens[i].type == TokenType::LBRACE || tokens[i].type == TokenType::LPAREN || tokens[i].type == TokenType::LBRACKET) {
                    groupsOpen.push_back(i);
                } else if (tokens[i].type == TokenType::RBRACE || tokens[i].type == TokenType::RPAREN || tokens[i].type == TokenType::RBRACKET) {
                    groupsOpen.pop_back();
                } else if (groupsOpen.size() == 1 && tokens[i].type == TokenType::COMMA) {
                    // split on commas, parse each subexpr
                    pArr->push( parseExpression(tokens, exprStart, i-1, scopeStack) );
                    exprStart = i+1;
                }
                ++i;
            }

            // set i back once
            --i;

            // verify brace is closed
            if (i > endIndex) throw TUnclosedGroupException(tokens[exprStart].err);

            // add last expression
            pArr->push( parseExpression(tokens, exprStart, i-1, scopeStack) );
        } else if (tokens[i].type == TokenType::LBRACKET) {
            // array subscript operator
            // find closing bracket
            size_t startBracket = i;
            size_t bracketsOpen = 0;

            do {
                if (tokens[i].type == TokenType::LBRACKET) bracketsOpen++;
                else if (tokens[i].type == TokenType::RBRACKET) bracketsOpen--;
                ++i;
            } while (i <= endIndex && bracketsOpen > 0);
            --i; // set i back once

            // verify closing bracket is found
            if (bracketsOpen > 0) throw TUnclosedGroupException(tokens[startIndex].err);

            // get previous identifier
            if (pHead->lastChild()->getNodeType() != ASTNodeType::IDENTIFIER)
                throw TInvalidTokenException(tokens[startIndex].err);
            ASTIdentifier& iden = *static_cast<ASTIdentifier*>(pHead->lastChild());

            // parse expression for subscript
            ASTArraySubscript* pArrSub = new ASTArraySubscript(tokens[startIndex]);
            iden.addSubscript( pArrSub );
            pArrSub->push( parseExpression(tokens, startBracket+1, i-1, scopeStack) );

            // force subscript to have int type
            ASTExpr* pSubExpr = static_cast<ASTExpr*>(pArrSub->lastChild());
            pSubExpr->type = Type(TokenType::TYPE_INT);
        } else {
            throw TInvalidTokenException(tokens[i].err);
        }
    }
}

void parsePrecedence2(const std::vector<Token>& tokens, ASTNode* pHead) {
    // combine unaries
    // precedence 2 (R -> L)
    for (long long i = pHead->size()-1; i >= 0; i--) {
        ASTNode& currentNode = *pHead->at(i);
        if (currentNode.getNodeType() != ASTNodeType::UNARY_OP && currentNode.getNodeType() != ASTNodeType::BIN_OP) continue;

        // ignore + and - if this is a binary math operation
        ASTOperator& currentOp = *static_cast<ASTOperator*>(&currentNode);
        TokenType opType = currentOp.getOpTokenType();

        // if it's a binary op, check for asterisk dereference or ampersand address operator
        if (!currentOp.getIsUnary() && opType != TokenType::ASTERISK && opType != TokenType::AMPERSAND)
            continue;

        // basically, unary can only work if it's after A) nothing or B) another operator
        if ((opType == TokenType::OP_ADD || opType == TokenType::OP_SUB ||
             opType == TokenType::ASTERISK || opType == TokenType::AMPERSAND) &&
            !(i == 0 || pHead->at(i-1)->getNodeType() == ASTNodeType::UNARY_OP ||
                        pHead->at(i-1)->getNodeType() == ASTNodeType::BIN_OP)) {
            continue;
        }

        // confirm there is a token after this
        if ((size_t)i+1 == pHead->size()) throw TInvalidTokenException(tokens[i].err);

        // if this is an asterisk, set it to a unary
        if (opType == TokenType::ASTERISK) {
            currentOp.setIsUnary(true);
        } else if (opType == TokenType::AMPERSAND) {
            currentOp.setIsUnary(true);
        }

        // append next node as child of this
        currentNode.push( pHead->at(i+1) );
        pHead->removeChild( i+1 );
    }
}

void parsePrecedence3(ASTNode* pHead) {
    // combine mult/div/mod
    // precedence 3 (L -> R)
    for (size_t i = 0; i < pHead->size(); i++) {
        ASTNode& currentNode = *pHead->at(i);
        if (currentNode.getNodeType() != ASTNodeType::BIN_OP) continue;

        // verify this is mult/div/mod math operation
        ASTOperator& currentOp = *static_cast<ASTOperator*>(&currentNode);
        TokenType opType = currentOp.getOpTokenType();
        if (opType != TokenType::ASTERISK && opType != TokenType::OP_DIV && opType != TokenType::OP_MOD)
            continue;
        
        // check for following expression
        if (i == 0 || i+1 == pHead->size()) throw TInvalidTokenException(currentNode.err);
        
        // append children
        currentNode.push(pHead->at(i-1));
        currentNode.push(pHead->at(i+1));
        pHead->removeChild(i+1); // remove last, first to prevent adjusting indexing
        pHead->removeChild(i-1);
        i--; // skip back once since removing previous node
    }
}

void parsePrecedence4(ASTNode* pHead) {
    // combine add/sub
    // precedence 4 (L -> R)
    for (size_t i = 0; i < pHead->size(); i++) {
        ASTNode& currentNode = *pHead->at(i);
        // + and - are counted as unaries by step 1
        if (currentNode.getNodeType() != ASTNodeType::UNARY_OP || currentNode.size() > 0) continue;

        // verify this is add/sub math operation
        ASTOperator& currentOp = *static_cast<ASTOperator*>(&currentNode);
        TokenType opType = currentOp.getOpTokenType();
        if (opType != TokenType::OP_ADD && opType != TokenType::OP_SUB) continue;

        // check for following expression
        if (i == 0 || i+1 == pHead->size()) throw TInvalidTokenException(currentNode.err);

        // convert ASTUnaryOp to ASTBinOp
        currentOp.setIsUnary(false);

        // append children
        currentNode.push(pHead->at(i-1));
        currentNode.push(pHead->at(i+1));
        pHead->removeChild(i+1); // remove last, first to prevent adjusting indexing
        pHead->removeChild(i-1);
        i--; // skip back once since removing previous node
    }
}

void parsePrecedence5(ASTNode* pHead) {
    // combine bitshifts
    // precedence 5 (L -> R)
    for (size_t i = 0; i < pHead->size(); i++) {
        ASTNode& currentNode = *pHead->at(i);
        if (currentNode.getNodeType() != ASTNodeType::BIN_OP) continue;

        // verify this is a comparison operation
        ASTOperator& currentOp = *static_cast<ASTOperator*>(&currentNode);
        if (currentOp.getOpTokenType() != TokenType::OP_LSHIFT && currentOp.getOpTokenType() != TokenType::OP_RSHIFT) continue;

        // check for following expression
        if (i == 0 || i+1 == pHead->size()) throw TInvalidTokenException(currentNode.err);

        // append previous and next nodes as children of bin expr
        currentNode.push(pHead->at(i-1));
        currentNode.push(pHead->at(i+1));
        pHead->removeChild(i+1); // remove last, first to prevent adjusting indexing
        pHead->removeChild(i-1);
        i--; // skip back once since removing previous node
    }
}

void parsePrecedence6(ASTNode* pHead) {
    // combine comparison GT/LT/GTE/LTE operators
    // precedence 6 (L -> R)
    for (size_t i = 0; i < pHead->size(); i++) {
        ASTNode& currentNode = *pHead->at(i);
        if (currentNode.getNodeType() != ASTNodeType::BIN_OP) continue;

        // verify this is either GT, GTE, LT<, or LTE
        ASTOperator& currentOp = *static_cast<ASTOperator*>(&currentNode);
        TokenType tokenType = currentOp.getOpTokenType();
        if (tokenType != TokenType::OP_GT && tokenType != TokenType::OP_GTE &&
            tokenType != TokenType::OP_LT && tokenType != TokenType::OP_LTE)
            continue;

        // check for following expression
        if (i == 0 || i+1 == pHead->size()) throw TInvalidTokenException(currentNode.err);

        // append previous and next nodes as children of bin expr
        currentNode.push(pHead->at(i-1));
        currentNode.push(pHead->at(i+1));
        pHead->removeChild(i+1); // remove last, first to prevent adjusting indexing
        pHead->removeChild(i-1);
        i--; // skip back once since removing previous node
    }
}

void parsePrecedence7(ASTNode* pHead) {
    // combine equality comparison operators
    // precedence 7 (L -> R)
    for (size_t i = 0; i < pHead->size(); i++) {
        ASTNode& currentNode = *pHead->at(i);
        if (currentNode.getNodeType() != ASTNodeType::BIN_OP) continue;

        // verify this is an equality comparison
        ASTOperator& currentOp = *static_cast<ASTOperator*>(&currentNode);
        TokenType tokenType = currentOp.getOpTokenType();
        if (tokenType != TokenType::OP_EQ && tokenType != TokenType::OP_NEQ)
            continue;

        // check for following expression
        if (i == 0 || i+1 == pHead->size()) throw TInvalidTokenException(currentNode.err);

        // append previous and next nodes as children of bin expr
        currentNode.push(pHead->at(i-1));
        currentNode.push(pHead->at(i+1));
        pHead->removeChild(i+1); // remove last, first to prevent adjusting indexing
        pHead->removeChild(i-1);
        i--; // skip back once since removing previous node
    }
}

void parsePrecedence8(ASTNode* pHead) {
    // combine bitwise and
    // precedence 8 (L -> R)
    for (size_t i = 0; i < pHead->size(); i++) {
        ASTNode& currentNode = *pHead->at(i);
        if (currentNode.getNodeType() != ASTNodeType::BIN_OP) continue;

        // verify this is bitwise and
        ASTOperator& currentOp = *static_cast<ASTOperator*>(&currentNode);
        TokenType tokenType = currentOp.getOpTokenType();
        if (tokenType != TokenType::AMPERSAND) continue;

        // check for following expression
        if (i == 0 || i+1 == pHead->size()) throw TInvalidTokenException(currentNode.err);

        // append previous and next nodes as children of bin expr
        currentNode.push(pHead->at(i-1));
        currentNode.push(pHead->at(i+1));
        pHead->removeChild(i+1); // remove last, first to prevent adjusting indexing
        pHead->removeChild(i-1);
        i--; // skip back once since removing previous node
    }
}

void parsePrecedence9(ASTNode* pHead) {
    // combine bitwise xor
    // precedence 9 (L -> R)
    for (size_t i = 0; i < pHead->size(); i++) {
        ASTNode& currentNode = *pHead->at(i);
        if (currentNode.getNodeType() != ASTNodeType::BIN_OP) continue;

        // verify this is bitwise xor
        ASTOperator& currentOp = *static_cast<ASTOperator*>(&currentNode);
        TokenType tokenType = currentOp.getOpTokenType();
        if (tokenType != TokenType::OP_BIT_XOR) continue;

        // check for following expression
        if (i == 0 || i+1 == pHead->size()) throw TInvalidTokenException(currentNode.err);

        // append previous and next nodes as children of bin expr
        currentNode.push(pHead->at(i-1));
        currentNode.push(pHead->at(i+1));
        pHead->removeChild(i+1); // remove last, first to prevent adjusting indexing
        pHead->removeChild(i-1);
        i--; // skip back once since removing previous node
    }
}

void parsePrecedence10(ASTNode* pHead) {
    // combine bitwise or
    // precedence 10 (L -> R)
    for (size_t i = 0; i < pHead->size(); i++) {
        ASTNode& currentNode = *pHead->at(i);
        if (currentNode.getNodeType() != ASTNodeType::BIN_OP) continue;

        // verify this is bitwise xor
        ASTOperator& currentOp = *static_cast<ASTOperator*>(&currentNode);
        TokenType tokenType = currentOp.getOpTokenType();
        if (tokenType != TokenType::OP_BIT_OR) continue;

        // check for following expression
        if (i == 0 || i+1 == pHead->size()) throw TInvalidTokenException(currentNode.err);

        // append previous and next nodes as children of bin expr
        currentNode.push(pHead->at(i-1));
        currentNode.push(pHead->at(i+1));
        pHead->removeChild(i+1); // remove last, first to prevent adjusting indexing
        pHead->removeChild(i-1);
        i--; // skip back once since removing previous node
    }
}

void parsePrecedence11(ASTNode* pHead) {
    // combine logical and
    // precedence 11 (L -> R)
    for (size_t i = 0; i < pHead->size(); i++) {
        ASTNode& currentNode = *pHead->at(i);
        if (currentNode.getNodeType() != ASTNodeType::BIN_OP) continue;

        // verify this is bitwise xor
        ASTOperator& currentOp = *static_cast<ASTOperator*>(&currentNode);
        TokenType tokenType = currentOp.getOpTokenType();
        if (tokenType != TokenType::OP_BOOL_AND) continue;

        // check for following expression
        if (i == 0 || i+1 == pHead->size()) throw TInvalidTokenException(currentNode.err);

        // append previous and next nodes as children of bin expr
        currentNode.push(pHead->at(i-1));
        currentNode.push(pHead->at(i+1));
        pHead->removeChild(i+1); // remove last, first to prevent adjusting indexing
        pHead->removeChild(i-1);
        i--; // skip back once since removing previous node
    }
}

void parsePrecedence12(ASTNode* pHead) {
    // combine logical or
    // precedence 12 (L -> R)
    for (size_t i = 0; i < pHead->size(); i++) {
        ASTNode& currentNode = *pHead->at(i);
        if (currentNode.getNodeType() != ASTNodeType::BIN_OP) continue;

        // verify this is bitwise xor
        ASTOperator& currentOp = *static_cast<ASTOperator*>(&currentNode);
        TokenType tokenType = currentOp.getOpTokenType();
        if (tokenType != TokenType::OP_BOOL_OR) continue;

        // check for following expression
        if (i == 0 || i+1 == pHead->size()) throw TInvalidTokenException(currentNode.err);

        // append previous and next nodes as children of bin expr
        currentNode.push(pHead->at(i-1));
        currentNode.push(pHead->at(i+1));
        pHead->removeChild(i+1); // remove last, first to prevent adjusting indexing
        pHead->removeChild(i-1);
        i--; // skip back once since removing previous node
    }
}

void parsePrecedence14(ASTNode* pHead) {
    // combine assignment operators
    // precedence 14 (R -> L)
    for (long long i = pHead->size()-1; i >= 0; i--) {
        ASTNode& currentNode = *pHead->at(i);
        if (currentNode.getNodeType() != ASTNodeType::BIN_OP) continue;

        // verify this is an assignment expression
        ASTOperator& currentOp = *static_cast<ASTOperator*>(&currentNode);
        if (!isTokenAssignOp(currentOp.getOpTokenType())) continue;

        // skip if already parsed
        if (currentOp.size() > 0) continue;

        // check for following expression
        if (i == 0 || (size_t)i+1 == pHead->size()) throw TInvalidTokenException(currentNode.err);
        
        // append previous and next nodes as children of bin expr
        currentNode.push(pHead->at(i-1));
        currentNode.push(pHead->at(i+1));
        pHead->removeChild(i+1); // remove last, first to prevent adjusting indexing
        pHead->removeChild(i-1);
        i--; // skip back once since removing previous node
    }
}
