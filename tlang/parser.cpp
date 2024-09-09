#include <vector>

#include "parser.hpp"
#include "toolbox.hpp"
#include "t_exception.hpp"
#include "ast/ast.hpp"

// takes in a vector of Tokens and assembles an AST for the document
AST* parseToAST(const std::vector<Token>& tokens) {
    // create the AST
    AST* pAST = new AST();

    // fill in the AST
    try {
        // only allow functions in the global scope
        size_t tokensLen = tokens.size();
        for (size_t i = 0; i < tokensLen; i++) {
            // parse the next function
            size_t startIndex = i, endIndex = i;

            // verify return type is specified
            if ( !isTokenTypeName(tokens[i].type) )
                throw TInvalidTokenException(tokens[i].err);
            
            // verify an identifier name is present
            if ( i+1 == tokensLen || tokens[++i].type != TokenType::IDENTIFIER )
                throw TInvalidTokenException(tokens[i].err);
            
            // verify opening parenthesis is present
            if ( i+1 == tokensLen || tokens[++i].type != TokenType::LPAREN )
                throw TInvalidTokenException(tokens[i].err);

            // verify closing parenthesis is present
            endIndex = i; // store brace start in endIndex for now <--------------
            size_t parensOpen = 1;
            do {
                i++;
                if (tokens[i].type == TokenType::LPAREN)
                    parensOpen++;
                else if (tokens[i].type == TokenType::RPAREN)
                    parensOpen--;
            } while ( parensOpen > 0 && i < tokensLen );

            if ( i == tokensLen ) throw TUnclosedGroupException(tokens[startIndex+2].err);

            // verify opening brace is present
            if ( i+1 == tokensLen || tokens[++i].type != TokenType::LBRACE )
                throw TInvalidTokenException(tokens[i].err);
            
            // verify closing brace is present
            endIndex = i; // store brace start in endIndex for now <--------------
            size_t bracesOpen = 1;
            do {
                i++;
                if (tokens[i].type == TokenType::LBRACE)
                    bracesOpen++;
                else if (tokens[i].type == TokenType::RBRACE)
                    bracesOpen--;
            } while ( bracesOpen > 0 && i < tokensLen );

            if ( i == tokensLen ) throw TUnclosedGroupException(tokens[endIndex].err);

            // all good to go
            endIndex = i;
            pAST->push( parseFunction(tokens, startIndex, endIndex) );
        }
    } catch (TException& e) {
        // free memory & rethrow
        delete pAST;
        throw e;
    }

    // return the AST
    return pAST;
}

/************************ FOR PARSING SPECIFIC ASTNodes ************************/

void parseBody(ASTNode* pHead, const std::vector<Token>& tokens, const size_t startIndex, const size_t endIndex) {
    // parse body lines and append to pHead
    for (size_t i = startIndex; i <= endIndex; i++) {
        switch (tokens[i].type) {
            case TokenType::IF: { // parse conditional
                bool isInConditional = true;
                size_t endCond = i;
                std::vector<size_t> branchIndices; // the start index for each branch

                while (isInConditional) {
                    // record this branch token index
                    branchIndices.push_back(endCond);

                    // only if this token isn't else, get a condition
                    if (tokens[endCond].type != TokenType::ELSE) {
                        // verify next token is LPAREN
                        if (endCond+1 == endIndex || tokens[++endCond].type != TokenType::LPAREN)
                            throw TInvalidTokenException(tokens[endCond].err);

                        // find closing RPAREN
                        size_t parenStart = endCond;
                        size_t parensOpen = 1;
                        do {
                            ++endCond;
                            if (tokens[endCond].type == TokenType::LPAREN)
                                parensOpen++;
                            else if (tokens[endCond].type == TokenType::RPAREN)
                                parensOpen--;
                        } while (parensOpen > 0 && endCond <= endIndex);

                        if (endCond > endIndex) throw TUnclosedGroupException(tokens[parenStart].err);
                    }

                    // verify next token is LBRACE
                    if (tokens[++endCond].type != TokenType::LBRACE)
                        throw TInvalidTokenException(tokens[endCond].err);

                    // find RBRACE
                    size_t braceStart = endCond;
                    size_t bracesOpen = 1;
                    do {
                        ++endCond;
                        if (tokens[endCond].type == TokenType::LBRACE)
                            bracesOpen++;
                        else if (tokens[endCond].type == TokenType::RBRACE)
                            bracesOpen--;
                    } while (bracesOpen > 0 && endCond <= endIndex);

                    if (endCond > endIndex) throw TUnclosedGroupException(tokens[braceStart].err);

                    // check for else on the same line
                    if (tokens[*branchIndices.rbegin()].type == TokenType::ELSE || // the current branch is an else (nothing allowed after)
                        endCond+1 == endIndex || // there's nothing after
                        (tokens[endCond+1].type != TokenType::ELSE_IF && tokens[endCond+1].type != TokenType::ELSE) // there's not an else if/else after
                    ) {
                        // break out of conditional
                        endCond--;
                        isInConditional = false;
                    }

                    // base case, recurse
                    ++endCond;
                }

                // parse conditional
                pHead->push( parseConditional(tokens, branchIndices, endCond) );
                i = endCond; // jump to end of conditional
                break;
            }
            case TokenType::FOR: { // parse for-loop
                size_t loopStart = i;
                // verify next token is LPAREN
                if (i+1 == endIndex || tokens[++i].type != TokenType::LPAREN)
                    throw TInvalidTokenException(tokens[i].err);

                // find closing RPAREN
                size_t parenStart = i;
                size_t parensOpen = 1;
                do {
                    ++i;
                    if (tokens[i].type == TokenType::LPAREN)
                        parensOpen++;
                    else if (tokens[i].type == TokenType::RPAREN)
                        parensOpen--;
                } while (parensOpen > 0 && i <= endIndex);

                if (i > endIndex) throw TUnclosedGroupException(tokens[parenStart].err);

                // verify next token is LBRACE
                if (tokens[++i].type != TokenType::LBRACE)
                    throw TInvalidTokenException(tokens[i].err);

                // find RBRACE
                size_t braceStart = i;
                size_t bracesOpen = 1;
                do {
                    ++i;
                    if (tokens[i].type == TokenType::LBRACE)
                        bracesOpen++;
                    else if (tokens[i].type == TokenType::RBRACE)
                        bracesOpen--;
                } while (bracesOpen > 0 && i <= endIndex);

                if (i > endIndex) throw TUnclosedGroupException(tokens[braceStart].err);

                // parse while loop
                pHead->push( parseForLoop(tokens, loopStart, i) );
                break;
            }
            case TokenType::WHILE: { // parse while-loop
                size_t loopStart = i;
                // verify next token is LPAREN
                if (i+1 == endIndex || tokens[++i].type != TokenType::LPAREN)
                    throw TInvalidTokenException(tokens[i].err);

                // find closing RPAREN
                size_t parenStart = i;
                size_t parensOpen = 1;
                do {
                    ++i;
                    if (tokens[i].type == TokenType::LPAREN)
                        parensOpen++;
                    else if (tokens[i].type == TokenType::RPAREN)
                        parensOpen--;
                } while (parensOpen > 0 && i <= endIndex);

                if (i > endIndex) throw TUnclosedGroupException(tokens[parenStart].err);

                // verify next token is LBRACE
                if (tokens[++i].type != TokenType::LBRACE)
                    throw TInvalidTokenException(tokens[i].err);

                // find RBRACE
                size_t braceStart = i;
                size_t bracesOpen = 1;
                do {
                    ++i;
                    if (tokens[i].type == TokenType::LBRACE)
                        bracesOpen++;
                    else if (tokens[i].type == TokenType::RBRACE)
                        bracesOpen--;
                } while (bracesOpen > 0 && i <= endIndex);

                if (i > endIndex) throw TUnclosedGroupException(tokens[braceStart].err);

                // parse while loop
                pHead->push( parseWhileLoop(tokens, loopStart, i) );
                break;
            }
            case TokenType::RETURN: { // parse return expression
                // create return
                ASTReturn* pReturn = new ASTReturn(tokens[i]);
                pHead->push(pReturn);

                // get expression
                size_t endExpr = i;
                do {
                    ++endExpr;
                } while (endExpr <= endIndex && tokens[endExpr].type != TokenType::SEMICOLON);
                
                // verify semicolon is present
                if (endExpr > endIndex) throw TInvalidTokenException(tokens[i].err);

                // append expression to pReturn
                pReturn->push( parseExpression(tokens, i+1, endExpr-1) );
                i = endExpr;
                break;
            }
            case TokenType::BLOCK_COMMENT_START: { // search for close
                size_t j = i;
                do {
                    j++;
                } while (j <= endIndex && tokens[j].type != TokenType::BLOCK_COMMENT_END);

                // handle unclosed comments
                if (j > endIndex) throw TUnclosedCommentException(tokens[i].err);

                // jump to end of comment
                i = j;
                break;
            }
            case TokenType::SEMICOLON: break; // erroneous statement
            default: { // base case, parse as expression
                // check for variable assignment
                if (isTokenPrimitiveType(tokens[i].type)) {
                    TokenType varType = tokens[i].type;
                    ASTVarDeclaration* pVarDec = new ASTVarDeclaration(tokens[i]);
                    pHead->push(pVarDec); // append here so it gets freed on its own if an error occurs here

                    // get identifier
                    if (i+1 > endIndex || tokens[++i].type != TokenType::IDENTIFIER)
                        throw TInvalidTokenException(tokens[i].err);

                    pVarDec->pIdentifier = new ASTIdentifier(tokens[i].raw, tokens[i]);

                    // verify not end of input
                    if (i+1 > endIndex) throw TInvalidTokenException(tokens[i].err);

                    // if declared but not assigned, leave pExpr as nullptr
                    if (tokens[++i].type == TokenType::SEMICOLON)
                        break;
                    else if (tokens[i].type != TokenType::ASSIGN) // variable must be assigned
                        throw TInvalidTokenException(tokens[i].err);

                    // get expression
                    size_t endExpr = i+1; // skip over assignment operator
                    while (endExpr <= endIndex && tokens[endExpr].type != TokenType::SEMICOLON)
                        ++endExpr;

                    // verify semicolon is present
                    if (endExpr > endIndex) throw TInvalidTokenException(tokens[i].err);
                    
                    // append expression (*always* returns an ASTExpr* as an ASTNode*)
                    pVarDec->pExpr = static_cast<ASTExpr*>(parseExpression(tokens, i+1, endExpr-1));
                    i = endExpr; // update `i` to position of semicolon
                    break;
                }

                // get expression
                size_t endExpr = i;
                while (endExpr <= endIndex && tokens[endExpr].type != TokenType::SEMICOLON)
                    ++endExpr;

                // verify semicolon is present
                if (endExpr > endIndex) throw TInvalidTokenException(tokens[i].err);
                
                pHead->push( parseExpression(tokens, i, endExpr-1) );
                i = endExpr;
                break;
            }
        }
    }
}

ASTNode* parseFunction(const std::vector<Token>& tokens, const size_t startIndex, const size_t endIndex) {
    // get function name
    const std::string name = tokens[startIndex+1].raw;

    // create node
    ASTFunction* pHead = new ASTFunction(name, tokens[startIndex]);

    try {
        // append parameters
        size_t i = startIndex+3;
        while (tokens[i].type != TokenType::RPAREN) {
            // verify param type is valid
            TokenType paramType = tokens[i].type;
            if ( !isTokenTypeName(paramType) )
                throw TInvalidTokenException(tokens[i].err);

            const std::string paramName = tokens[++i].raw; // grab parameter name
            pHead->appendParam({paramName, paramType}); // append parameter

            if (tokens[i+1].type == TokenType::COMMA) i++; // skip next comma
            i++; // base increment
        }

        // verify opening brace is next
        if (tokens[++i].type != TokenType::LBRACE)
            throw TInvalidTokenException(tokens[i].err);
        
        // parse body (up to but not including closing brace)
        parseBody(pHead, tokens, i+1, endIndex-1);
    } catch (TException& e) {
        delete pHead; // free & rethrow
        throw e;
    }

    // return node ptr
    return pHead;
}

ASTNode* parseExpression(const std::vector<Token>& tokens, const size_t startIndex, const size_t endIndex) {
    if (startIndex > endIndex) throw TInvalidTokenException(tokens[endIndex].err);

    ASTNode* pHead = new ASTExpr(tokens[startIndex]);

    // iterate through expression
    try {
        // order of operations for expression parsing
        // (bottom)     -->     -->     -->     -->     -->     -->     -->     (top)
        // PARENTHESIS, UNARIES, MULT/DIV/MOD, ADD/SUB & INC/DEC, COMPARISON, ASSIGNMENT
        /**
         * 1. PARSE ALL TOKENS (W/ PARENTHESIS RECURSIVELY) SO NODE IS FLAT EXCEPT FOR PARENTHETICALS
         * 2. COMBINE UNARIES
         * 3. COMBINE MULT/DIV/MOD ARGS ON EITHER SIDE
         * 4. ADD/SUB
         * 5. SHIFTS
         * 6. COMPARISON
         * 7. ASSIGNMENT
        */

        // 1. PARSE ALL TOKENS (W/ PARENTHESIS RECURSIVELY) SO NODE IS FLAT EXCEPT FOR PARENTHETICALS
        for (size_t i = startIndex; i <= endIndex; i++) {
            if (tokens[i].type == TokenType::LPAREN) { // find closing parenthesis
                if (i+1 > endIndex) throw TUnclosedGroupException(tokens[i].err);

                size_t start = i, parensOpen = 1;
                do {
                    if (tokens[++i].type == TokenType::RPAREN)
                        parensOpen--;
                    else if (tokens[i].type == TokenType::LPAREN)
                        parensOpen++;
                } while (i <= endIndex && parensOpen > 0);

                if (i > endIndex) throw TUnclosedGroupException(tokens[start].err);

                pHead->push( parseExpression(tokens, start+1, i-1) ); // recurse
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
            } else if (tokens[i].type == TokenType::VOID) {
                pHead->push( new ASTVoidLiteral(tokens[i]) );
            } else if (isTokenUnaryOp(tokens[i].type)) { // handle unary operators
                pHead->push( new ASTOperator(tokens[i], true) );
            } else if (isTokenBinaryOp(tokens[i].type)) { // handle binary operators
                pHead->push( new ASTOperator(tokens[i], false) );
            } else if (tokens[i].type == TokenType::IDENTIFIER) {
                pHead->push( new ASTIdentifier(tokens[i].raw, tokens[i]) );
            } else {
                throw TInvalidTokenException(tokens[i].err);
            }
        }

        // 2. COMBINE UNARIES
        for (size_t i = 0; i < pHead->size(); i++) {
            ASTNode& currentNode = *pHead->at(i);
            if (currentNode.getNodeType() != ASTNodeType::UNARY_OP) continue;

            // ignore + and - if this is a binary math operation
            ASTOperator& currentOp = *static_cast<ASTOperator*>(&currentNode);
            TokenType opType = currentOp.getOpTokenType();

            // basically, unary can only work if it's after A) nothing or B) another operator
            if ((opType == TokenType::OP_ADD || opType == TokenType::OP_SUB) &&
                !(i == 0 || pHead->at(i-1)->getNodeType() == ASTNodeType::UNARY_OP ||
                            pHead->at(i-1)->getNodeType() == ASTNodeType::BIN_OP)) {
                continue;
            }

            // confirm there is a token after this
            if (i+1 == pHead->size()) throw TInvalidTokenException(tokens[i].err);

            // append next node as child of this
            currentNode.push( pHead->at(i+1) );
            pHead->removeChild( i+1 );
        }

        // 3. COMBINE MULT/DIV/MOD ARGS ON EITHER SIDE
        for (size_t i = 0; i < pHead->size(); i++) {
            ASTNode& currentNode = *pHead->at(i);
            if (currentNode.getNodeType() != ASTNodeType::BIN_OP) continue;

            // verify this is mult/div/mod math operation
            ASTOperator* currentOp = static_cast<ASTOperator*>(&currentNode);
            TokenType opType = currentOp->getOpTokenType();
            if (opType != TokenType::OP_MUL && opType != TokenType::OP_DIV && opType != TokenType::OP_MOD)
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

        // 4. ADD/SUB
        for (size_t i = 0; i < pHead->size(); i++) {
            ASTNode& currentNode = *pHead->at(i);
            // + and - are counted as unaries by step 1
            if (currentNode.getNodeType() != ASTNodeType::UNARY_OP) continue;

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

        // 5. SHIFTS
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

        // 6. COMPARISON
        for (size_t i = 0; i < pHead->size(); i++) {
            ASTNode& currentNode = *pHead->at(i);
            if (currentNode.getNodeType() != ASTNodeType::BIN_OP) continue;

            // verify this is a comparison operation
            ASTOperator& currentOp = *static_cast<ASTOperator*>(&currentNode);
            if (!isTokenCompOp(currentOp.getOpTokenType())) continue;

            // check for following expression
            if (i == 0 || i+1 == pHead->size()) throw TInvalidTokenException(currentNode.err);

            // append previous and next nodes as children of bin expr
            currentNode.push(pHead->at(i-1));
            currentNode.push(pHead->at(i+1));
            pHead->removeChild(i+1); // remove last, first to prevent adjusting indexing
            pHead->removeChild(i-1);
            i--; // skip back once since removing previous node
        }

        // 7. ASSIGNMENT
        for (size_t i = 0; i < pHead->size(); i++) {
            ASTNode& currentNode = *pHead->at(i);
            if (currentNode.getNodeType() != ASTNodeType::BIN_OP) continue;

            // verify this is an assignment expression
            ASTOperator& currentOp = *static_cast<ASTOperator*>(&currentNode);
            if (!isTokenAssignOp(currentOp.getOpTokenType())) continue;

            // check for following expression
            if (i == 0 || i+1 == pHead->size()) throw TInvalidTokenException(currentNode.err);
            
            // append previous and next nodes as children of bin expr
            currentNode.push(pHead->at(i-1));
            currentNode.push(pHead->at(i+1));
            pHead->removeChild(i+1); // remove last, first to prevent adjusting indexing
            pHead->removeChild(i-1);
            i--; // skip back once since removing previous node
        }
    } catch (TException& e) {
        // free & rethrow
        delete pHead;
        throw e;
    }

    return pHead;
}

ASTNode* parseConditional(const std::vector<Token>& tokens, const std::vector<size_t>& branchIndices, const size_t globalEndIndex) {
    ASTNode* pHead = new ASTConditional(tokens[branchIndices[0]]);

    try {
        // iterate for each branch
        size_t numBranches = branchIndices.size();
        for (size_t i = 0; i < numBranches; i++) {
            // determine start and end indices
            size_t startIndex = branchIndices[i];
            size_t endIndex = i+1 == numBranches ? globalEndIndex : branchIndices[i+1]-1;

            // parse condition
            size_t bodyStart = startIndex + 2; // base case, after LBRACE
            const Token& startToken = tokens[startIndex];
            ASTNode* pNode;
            if (startToken.type != TokenType::ELSE) {
                // append conditional node
                pNode = startToken.type == TokenType::IF ?
                                    (ASTNode*) new ASTIfCondition(startToken) : (ASTNode*) new ASTElseIfCondition(startToken);
                pHead->push( pNode );

                // find closing condition
                size_t openBrace = startIndex+1;
                do { openBrace++; } while (tokens[openBrace].type != TokenType::LBRACE);

                // prevent empty expressions
                if (startIndex+2 > openBrace-2) throw TInvalidTokenException(tokens[startIndex+1].err);

                // append expression
                if (startToken.type == TokenType::IF) {
                    static_cast<ASTIfCondition*>(pNode)->pExpr = parseExpression(tokens, startIndex+2, openBrace-2);
                } else {
                    static_cast<ASTElseIfCondition*>(pNode)->pExpr = parseExpression(tokens, startIndex+2, openBrace-2);
                }

                // offset the body token position
                bodyStart = openBrace+1;
            } else {
                pNode = new ASTElseCondition(startToken);
                pHead->push( pNode );
            }

            // parse the body (up to but not including closing brace)
            parseBody(pNode, tokens, bodyStart, endIndex-1);
        }
    } catch (TException& e) {
        // free & rethrow
        delete pHead;
        throw e;
    }

    return pHead;
}

ASTNode* parseWhileLoop(const std::vector<Token>& tokens, const size_t startIndex, const size_t endIndex) {
    // create new node
    ASTWhileLoop* pHead = new ASTWhileLoop(tokens[startIndex]);

    try {
        // append expression
        size_t i = startIndex;

        // find LBRACE
        do { ++i; } while (tokens[i].type != TokenType::LBRACE);

        // parse expression
        pHead->pExpr = parseExpression( tokens, startIndex+2, i-2); // ignore parenthesis

        // parse body
        parseBody( pHead, tokens, i+1, endIndex-1 ); // ignore braces
    } catch (TException& e) {
        delete pHead;
        throw e;
    }

    // return node
    return pHead;
}

ASTNode* parseForLoop(const std::vector<Token>& tokens, const size_t startIndex, const size_t endIndex) {
    // create new node
    ASTForLoop* pHead = new ASTForLoop(tokens[startIndex]);

    try {
        // append sub-expressions
        size_t i = startIndex;
        long long semiA = -1, semiB =- 1; // only allowed two semicolons in for-loop header (ex. for (...;...;...) )

        // find LBRACE
        do {
            ++i;
            if (tokens[i].type == TokenType::SEMICOLON) {
                if (semiA == -1)
                    semiA = i;
                else if (semiB == -1)
                    semiB = i;
                else
                    throw TInvalidTokenException(tokens[i].err);
            }
        } while (tokens[i].type != TokenType::LBRACE);

        // parse sub-expressions on semicolons
        pHead->pExprA = parseExpression( tokens, startIndex+2, semiA-1); // ignore opening parenthesis & semicolon
        pHead->pExprB = parseExpression( tokens, semiA+1, semiB-1); // ignore parenthesis & semicolon
        pHead->pExprC = parseExpression( tokens, semiB+1, i-2); // ignore closing parenthesis

        // parse body
        parseBody( pHead, tokens, i+1, endIndex-1 ); // ignore braces
    } catch (TException& e) {
        delete pHead;
        throw e;
    }

    // return node
    return pHead;
}