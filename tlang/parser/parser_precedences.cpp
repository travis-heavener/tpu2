#include <stack>
#include <vector>

#include "parser_precedences.hpp"
#include "parser.hpp"

#include "../util/token.hpp"
#include "../util/toolbox.hpp"
#include "../util/t_exception.hpp"
#include "../util/scope_stack.hpp"
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

            // A. check for function call
            if (start - 1 >= startIndex && tokens[start-1].type == TokenType::IDENTIFIER) {
                // create function call node
                ASTFunctionCall* pCall = new ASTFunctionCall(tokens[start-1]);
                pHead->push( pCall ); // append function call node

                // check for arguments
                if (start+1 <= i-1) {
                    // find commas
                    std::vector<size_t> commaIndices;
                    delimitIndices(tokens, commaIndices, start+1, i-1); // ignore outer parens

                    // append sub expressions separated by a comma
                    size_t subExprStart = start+1;
                    for (size_t j = 0; j < commaIndices.size(); ++j) {
                        pCall->push( parseExpression(tokens, subExprStart, commaIndices[j] - 1, scopeStack, true) );
                        subExprStart = commaIndices[j] + 1; // update for next sub expr
                    }

                    // check for remaining expression
                    if (subExprStart != i)
                        pCall->push( parseExpression(tokens, subExprStart, i-1, scopeStack, true) );
                }
            } else if (start + 1 <= endIndex && isTokenTypeKeyword(tokens[start+1].type)) { // B. check for typecast
                // grab type
                Type type( TokenType::TYPE_INT ); // default to signed int

                // while we have a type keyword, modify the type
                size_t j = start+1;
                for ((void)j; j < i; ++j) {
                    if (isTokenTypeKeyword(tokens[j].type)) {
                        // check const if first char
                        if (j == start+1 && tokens[j].type == TokenType::CONST) {
                            type.setIsConst(true);
                        } else if (isTokenSignedUnsigned(tokens[j].type)) {
                            type.setIsUnsigned(tokens[j].type == TokenType::UNSIGNED);
                        } else if (isTokenPrimitiveType(tokens[j].type, true)) {
                            type.setPrimType(tokens[j].type);
                            ++j;
                            break; // primitive must be last
                        } else {
                            // invalid token
                            throw TInvalidTokenException(tokens[j].err);
                        }
                    } else {
                        break; // not a type keyword, but might be a pointer
                    }
                }

                // grab pointers
                for ((void)j; j < i; ++j) {
                    // only accept asterisk
                    if (tokens[j].type != TokenType::ASTERISK)
                        throw TInvalidTokenException(tokens[j].err);

                    // add to type
                    type.addEmptyPointer();
                }

                // only allow unsigned int or char, and disallow const void
                bool isInvalidUnsigned = type.isUnsigned() && type.getPrimType() != TokenType::TYPE_INT && type.getPrimType() != TokenType::TYPE_CHAR;
                if (isInvalidUnsigned || (type.isVoidNonPtr() && type.isConst())) {
                    throw TSyntaxException(tokens[j].err);
                }

                // append typecast
                ASTTypeCast* pTypeCast = new ASTTypeCast(tokens[start]);
                pHead->push( pTypeCast );
                pTypeCast->setType( type );
            } else { // C. found a subexpression
                // if the previous node is a protected asm instruction, append to that instead
                if (pHead->size() > 0 && pHead->lastChild()->getNodeType() == ASTNodeType::ASM_INST) {
                    // parse sub expression, if present
                    if (start+1 <= i-1)
                        pHead->lastChild()->push( parseExpression(tokens, start+1, i-1, scopeStack) );
                } else {
                    pHead->push( parseExpression(tokens, start+1, i-1, scopeStack) );
                }
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
            // get raw string text w/o quotes
            const std::string rawString = tokens[i].raw.substr(1, tokens[i].raw.size()-2);

            // create string literal
            ASTStringLiteral* pStrLit = new ASTStringLiteral(tokens[i], rawString);
            pHead->push(pStrLit);
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
                    // split on commas, parse each subexpr as its own top expression
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

            // get previous node
            if (pHead->size() == 0) throw TInvalidOperationException(tokens[startBracket].err);
            ASTTypedNode* pLastNode = dynamic_cast<ASTTypedNode*>(pHead->lastChild());
            if (pLastNode == nullptr) // failed to cast to subscriptable type
                throw TInvalidOperationException(tokens[startIndex].err);

            // parse expression for subscript
            ASTArraySubscript* pArrSub = new ASTArraySubscript(tokens[startBracket]);
            pLastNode->addSubscript( pArrSub );
            pArrSub->push( parseExpression(tokens, startBracket+1, i-1, scopeStack, true) );

            // force subscript to have int type
            ASTExpr* pSubExpr = static_cast<ASTExpr*>(pArrSub->lastChild());
            pSubExpr->setType( Type(TokenType::TYPE_INT) );
        } else if (tokens[i].type == TokenType::ASM) {
            // create an inline asm node and parse the attached string
            if (i+3 > endIndex || tokens[i+1].type != TokenType::LPAREN
                || tokens[i+2].type != TokenType::LIT_STRING || tokens[i+3].type != TokenType::RPAREN)
                throw TSyntaxException(tokens[i+1].err);

            // evaluate the inline assembly
            const std::string inlineASM = tokens[i+2].raw.substr(1, tokens[i+2].raw.size()-2);

            // create node
            pHead->push( new ASTInlineASM(tokens[i], inlineASM) );
            i += 3;
        } else if (isTokenProtectedASM(tokens[i].type)) {
            pHead->push( new ASMProtectedInstruction(tokens[i]) );
        } else {
            throw TInvalidTokenException(tokens[i].err);
        }
    }
}

void parsePrecedence2(const std::vector<Token>& tokens, ASTNode* pHead) {
    // combine unaries
    // precedence 2 (R -> L)
    for (long long i = pHead->size()-1; i >= 0; --i) {
        ASTNode& currentNode = *pHead->at(i);
        if (currentNode.getNodeType() == ASTNodeType::TYPE_CAST) { // handle typecasts
            // check if the previous node is sizeof
            ASTTypeCast* pTypeCast = static_cast<ASTTypeCast*>(&currentNode);
            if (i > 0) {
                ASTOperator* pPrevOp = dynamic_cast<ASTOperator*>(pHead->at(i-1));
                if (pPrevOp != nullptr && pPrevOp->getOpTokenType() == TokenType::SIZEOF) {
                    // bind this as the child of sizeof
                    pPrevOp->push(
                        // add 0 to typecast to make it work
                        pTypeCast->toOperator(new ASTIntLiteral(0, pTypeCast->getToken()))
                    );
                    pHead->removeChild( i );
                    --i; // skip next node too
                    delete pTypeCast;
                    continue;
                }
            }

            // check for next token
            if ((size_t)i+1 == pHead->size())
                throw TInvalidTokenException(tokens[i].err);

            ASTOperator* pOp = pTypeCast->toOperator(pHead->at(i+1));
            pHead->removeChild(i+1); // remove appended child

            // replace self in parent
            pHead->removeChild(i);
            pHead->insert(pOp, i);
            delete pTypeCast;
            continue;
        }

        // handle other unaries
        if (currentNode.getNodeType() != ASTNodeType::UNARY_OP && currentNode.getNodeType() != ASTNodeType::BIN_OP) continue;

        // verify there aren't already children from recursing
        if (currentNode.size() != 0) continue;

        // ignore + and - if this is a binary math operation
        ASTOperator& currentOp = *static_cast<ASTOperator*>(&currentNode);
        TokenType opType = currentOp.getOpTokenType();

        // if it's a binary op, check for asterisk dereference or ampersand address operator
        if (!currentOp.getIsUnary() && opType != TokenType::ASTERISK && opType != TokenType::AMPERSAND)
            continue;

        // prevent chaining of + or - unaries (allow if previous node has children already)
        if ((opType == TokenType::OP_ADD || opType == TokenType::OP_SUB) && i > 0) {
            ASTOperator* pPrev = dynamic_cast<ASTOperator*>(pHead->at(i-1));
            if (pPrev != nullptr && pPrev->getOpTokenType() == opType && pPrev->size() == 0) {
                throw TInvalidTokenException(currentNode.err);
            }

            // verify the previous node is an operator (if not, it's a binary op)
            ASTTypeCast* pPrevTypeCast = dynamic_cast<ASTTypeCast*>(pHead->at(i-1));
            if (pPrev == nullptr && pPrevTypeCast == nullptr) continue;

            // verify that if the previous node is a typecast that it's not the child of sizeof
            if (pPrevTypeCast != nullptr && i-1 > 0 && pHead->at(i-2)->getNodeType() == ASTNodeType::UNARY_OP) {
                if (static_cast<ASTOperator*>(pHead->at(i-2))->getOpTokenType() == TokenType::SIZEOF) {
                    continue;
                }
            }
        } else if ((opType == TokenType::ASTERISK || opType == TokenType::AMPERSAND) && i > 0) {
            // verify the previous node is an operator OR typecast (if not, it's a binary op)
            ASTOperator* pPrev = dynamic_cast<ASTOperator*>(pHead->at(i-1));
            ASTTypeCast* pPrevTypeCast = dynamic_cast<ASTTypeCast*>(pHead->at(i-1));
            if (pPrev == nullptr && pPrevTypeCast == nullptr) continue;

            // verify that if the previous node is a typecast that it's not the child of sizeof
            if (pPrevTypeCast != nullptr && i-1 > 0 && pHead->at(i-2)->getNodeType() == ASTNodeType::UNARY_OP) {
                if (static_cast<ASTOperator*>(pHead->at(i-2))->getOpTokenType() == TokenType::SIZEOF) {
                    continue;
                }
            }
        }

        // confirm there is a token after this
        if ((size_t)i+1 == pHead->size()) throw TInvalidTokenException(tokens[i].err);

        // if this is also a binary op, set it to a unary
        if (opType == TokenType::ASTERISK || opType == TokenType::AMPERSAND)
            currentOp.setIsUnary(true);

        // update unary type
        if (opType == TokenType::SIZEOF) {
            currentOp.setUnaryType(ASTUnaryType::SIZEOF);
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

        // verify there aren't already children from recursing
        if (currentNode.size() != 0) continue;

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

        // verify there aren't already children from recursing
        if (currentNode.size() != 0) continue;

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

        // verify there aren't already children from recursing
        if (currentNode.size() != 0) continue;

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

        // verify there aren't already children from recursing
        if (currentNode.size() != 0) continue;

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

        // verify there aren't already children from recursing
        if (currentNode.size() != 0) continue;

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
        
        // verify there aren't already children from recursing
        if (currentNode.size() != 0) continue;

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

        // verify there aren't already children from recursing
        if (currentNode.size() != 0) continue;

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

        // verify there aren't already children from recursing
        if (currentNode.size() != 0) continue;

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

        // verify there aren't already children from recursing
        if (currentNode.size() != 0) continue;

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

        // verify there aren't already children from recursing
        if (currentNode.size() != 0) continue;

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

        // verify there aren't already children from recursing
        if (currentNode.size() != 0) continue;

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
