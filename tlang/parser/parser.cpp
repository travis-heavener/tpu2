#include <map>
#include <stack>
#include <vector>

#include "parser.hpp"
#include "parser_precedences.hpp"

#include "../util/util.hpp"
#include "../ast/ast.hpp"
#include "../ast/ast_nodes.hpp"

// takes in a vector of Tokens and assembles an AST for the document
AST* parseToAST(const std::vector<Token>& tokens) {
    // create the AST
    AST* pAST = new AST();

    // create a scope stack to store variables and functions' types
    scope_stack_t scopeStack = { parser_scope_t() }; // global scope

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
            pAST->push( parseFunction(tokens, startIndex, endIndex, scopeStack) );
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

void parseBody(ASTNode* pHead, const std::vector<Token>& tokens, const size_t startIndex, const size_t endIndex, scope_stack_t& scopeStack) {
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
                pHead->push( parseConditional(tokens, branchIndices, endCond, scopeStack) );
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
                pHead->push( parseForLoop(tokens, loopStart, i, scopeStack) );
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
                pHead->push( parseWhileLoop(tokens, loopStart, i, scopeStack) );
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
                pReturn->push( parseExpression(tokens, i+1, endExpr-1, scopeStack) );
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
                    size_t start = i;

                    // verify not end of input
                    if (i+1 > endIndex) throw TInvalidTokenException(tokens[i].err);

                    // get type
                    Type type(tokens[start].type);

                    // get identifier
                    if (tokens[++i].type != TokenType::IDENTIFIER)
                        throw TInvalidTokenException(tokens[i].err);
                    size_t idenStart = i;

                    // get all array modifiers (only allow integer sizes)
                    if (tokens[++i].type == TokenType::LBRACKET) {
                        size_t j;
                        size_t numModifiers = 0;
                        for (j = i; j <= endIndex && tokens[j].type == TokenType::LBRACKET; (void)j) {
                            if (tokens[j+1].type == TokenType::LIT_INT || numModifiers > 0) {
                                // verify next token is an int literal
                                if (tokens[j+1].type != TokenType::LIT_INT)
                                    throw TInvalidTokenException(tokens[i+1].err);

                                // verify next token is an RBRACKET
                                if (tokens[j+2].type != TokenType::RBRACKET)
                                    throw TInvalidTokenException(tokens[j+2].err);

                                // add with value otherwise
                                type.addArrayModifier( std::stol(tokens[j+1].raw) );
                                j += 3;
                            } else {
                                // verify next token is an RBRACKET
                                if (tokens[j+1].type != TokenType::RBRACKET)
                                    throw TInvalidTokenException(tokens[j+1].err);

                                // add empty array modifier if first bracket pair
                                type.addEmptyArrayModifier();
                                j += 2;
                            }
                            numModifiers++;
                        }
                        i = j;
                    }

                    // create node
                    ASTVarDeclaration* pVarDec = new ASTVarDeclaration(tokens[start], type);
                    pHead->push(pVarDec); // append here so it gets freed on its own if an error occurs here

                    // create identifier node (always an assignment operator)
                    pVarDec->pIdentifier = new ASTIdentifier(tokens[idenStart], true);

                    // add variable to scopeStack
                    declareParserVariable(scopeStack, tokens[idenStart].raw, type, tokens[idenStart].err);

                    // if declared but not assigned, leave pExpr as nullptr
                    if (tokens[i].type == TokenType::SEMICOLON) {
                        // verify that any subscripts for an array type are provided
                        if (type.hasEmptyArrayModifiers())
                            throw TInvalidTokenException(tokens[start].err);
                        break;
                    } else if (tokens[i].type != TokenType::ASSIGN) { // variable must be assigned
                        throw TInvalidTokenException(tokens[i].err);
                    }

                    // get expression
                    size_t endExpr = i+1; // skip over assignment operator
                    while (endExpr <= endIndex && tokens[endExpr].type != TokenType::SEMICOLON)
                        ++endExpr;

                    // verify semicolon is present
                    if (endExpr > endIndex) throw TInvalidTokenException(tokens[i].err);
                    
                    // append expression (*always* returns an ASTExpr* as an ASTNode*)
                    ASTExpr* pExpr = static_cast<ASTExpr*>(parseExpression(tokens, i+1, endExpr-1, scopeStack));
                    pVarDec->pExpr = pExpr;
                    i = endExpr; // update `i` to position of semicolon

                    // update array literal's type
                    if (type.isArray()) {
                        if (pExpr->at(0)->getNodeType() == ASTNodeType::LIT_ARR) {
                            ASTArrayLiteral* pArrLit = static_cast<ASTArrayLiteral*>(pExpr->at(0));

                            // verify all arguments have the same type
                            Type arrType = pArrLit->inferType(scopeStack);
                            arrType.flipModifiers(); // inferred are backwards
                            pArrLit->setType( arrType );
                            pExpr->type = arrType;
                        } else if (pExpr->at(0)->getNodeType() == ASTNodeType::IDENTIFIER) {
                            pExpr->type = type;
                        }
                    }

                    // confirm assignment type matches
                    if (!type.checkArrayMods(pExpr->type)) {
                        throw TSyntaxException(tokens[idenStart].err);
                    } else if (type.isArray() && type.getArrayModifiers()[0] == TYPE_NO_ARR_SIZE) {
                        // imply size from literal's type
                        type.flipModifiers(); // flip to make this easier
                        type.popArrayModifier();
                        type.addArrayModifier( pExpr->type.getArrayModifiers()[0] );
                        type.flipModifiers(); // unflip

                        ASTArrayLiteral* pArrLit = static_cast<ASTArrayLiteral*>(pExpr->at(0));
                        pArrLit->setType( type );
                        pExpr->type = type;
                        pVarDec->type = type;
                    }
                    break;
                }

                // base case, parse as an expression
                size_t endExpr = i;
                while (endExpr <= endIndex && tokens[endExpr].type != TokenType::SEMICOLON)
                    ++endExpr;

                // verify semicolon is present
                if (endExpr > endIndex) throw TInvalidTokenException(tokens[i].err);
                
                pHead->push( parseExpression(tokens, i, endExpr-1, scopeStack) );
                i = endExpr;
                break;
            }
        }
    }
}

ASTNode* parseFunction(const std::vector<Token>& tokens, const size_t startIndex, const size_t endIndex, scope_stack_t& scopeStack) {
    // get return type
    Type type(tokens[startIndex].type);

    // get all array modifiers (only allow integer sizes)
    size_t i = startIndex;
    if (tokens[++i].type == TokenType::LBRACKET) {
        for ((void)i; tokens[i].type == TokenType::LBRACKET; i += 3) {
            // verify next token is an int literal
            if (tokens[i+1].type != TokenType::LIT_INT)
                throw TInvalidTokenException(tokens[i+1].err);

            // verify token after literal is an RBRACKET
            if (tokens[i+2].type != TokenType::RBRACKET)
                throw TInvalidTokenException(tokens[i+2].err);

            // add array modifier
            type.addArrayModifier(std::stol(tokens[i+1].raw));
        }
    }

    // get function name
    const std::string name = tokens[i++].raw;

    // create node
    ASTFunction* pHead = new ASTFunction(name, tokens[startIndex], type);

    // put this function into the global scope
    declareParserVariable(scopeStack, name, type, tokens[startIndex].err);

    // create a new scope stack for scoped parser variables
    scopeStack.push_back( parser_scope_t() );

    try {
        // skip opening parenthesis
        ++i;

        // append parameters
        while (tokens[i].type != TokenType::RPAREN) {
            // get param type (checks the type)
            Type type( tokens[i].type );
            
            size_t idenIndex = ++i;

            // skip identifier
            ++i;

            // get all array modifiers
            size_t numModifiers = 0;
            while (tokens[i].type == TokenType::LBRACKET) {
                if (tokens[i+1].type == TokenType::LIT_INT || numModifiers > 0) {
                    // verify next token is an int literal
                    if (tokens[i+1].type != TokenType::LIT_INT)
                        throw TInvalidTokenException(tokens[i+1].err);

                    // verify next token is an RBRACKET
                    if (tokens[i+2].type != TokenType::RBRACKET)
                        throw TInvalidTokenException(tokens[i+2].err);

                    // add with value otherwise
                    type.addArrayModifier( std::stol(tokens[i+1].raw) );
                    i += 3;
                } else {
                    // verify next token is an RBRACKET
                    if (tokens[i+1].type != TokenType::RBRACKET)
                        throw TInvalidTokenException(tokens[i+1].err);

                    // add empty array modifier if first bracket pair
                    type.addEmptyArrayModifier();
                    i += 2;
                }
                numModifiers++;
            }

            // grab parameter name
            if (tokens[idenIndex].type != TokenType::IDENTIFIER)
                throw TInvalidTokenException(tokens[idenIndex].err);

            const std::string paramName = tokens[idenIndex].raw;
            ASTFuncParam* pParam = new ASTFuncParam(paramName, type);
            pHead->appendParam(pParam); // append parameter

            // add argument to scopeStack
            declareParserVariable(scopeStack, tokens[idenIndex].raw, type, tokens[idenIndex].err);

            if (tokens[i].type == TokenType::COMMA) i++; // skip next comma
        }

        // verify opening brace is next
        if (tokens[++i].type != TokenType::LBRACE)
            throw TInvalidTokenException(tokens[i].err);

        // parse body (up to but not including closing brace)
        parseBody(pHead, tokens, i+1, endIndex-1, scopeStack);
    } catch (TException& e) {
        delete pHead; // free & rethrow
        throw e;
    }

    // return node ptr
    return pHead;
}

ASTNode* parseExpression(const std::vector<Token>& tokens, const size_t startIndex, const size_t endIndex, scope_stack_t& scopeStack) {
    if (startIndex > endIndex) throw TInvalidTokenException(tokens[endIndex].err);

    ASTExpr* pHead = new ASTExpr(tokens[startIndex]);

    // iterate through expression
    try {
        // parsing all tokens directly (recursive for subexpressions) -- (L -> R)
        parsePrecedence1(tokens, startIndex, endIndex, pHead, scopeStack);
        
        parsePrecedence2(tokens, pHead); // combine unaries -- (R -> L)
        parsePrecedence3(pHead); // combine mult/div/mod -- (L -> R)
        parsePrecedence4(pHead); // combine add/sub -- (L -> R)
        parsePrecedence5(pHead); // combine bitshifts -- (L -> R)
        parsePrecedence6(pHead); // combine comparison GT/LT/GTE/LTE operators -- (L -> R)
        parsePrecedence7(pHead); // combine equality comparison operators -- (L -> R)
        parsePrecedence8(pHead); // combine bitwise and -- (L -> R)
        parsePrecedence9(pHead); // combine bitwise xor -- (L -> R)
        parsePrecedence10(pHead); // combine bitwise or -- (L -> R)
        parsePrecedence11(pHead); // combine logical and -- (L -> R)
        parsePrecedence12(pHead); // combine logical or -- (L -> R)
        parsePrecedence14(pHead); // combine assignment operators -- (R -> L)

        // final checks
        const size_t numChildren = pHead->size();
        for (size_t i = 0; i < numChildren; i++) {
            ASTNode& node = *pHead->at(i);
            ASTNodeType nodeType = node.getNodeType();

            // 1. determine the type of all operation nodes & identifiers (anything not a literal)
            if (nodeType == ASTNodeType::BIN_OP || nodeType == ASTNodeType::UNARY_OP) {
                ASTOperator& op = *static_cast<ASTOperator*>(&node);
                op.determineResultType(scopeStack);
            } else if (nodeType == ASTNodeType::IDENTIFIER) {
                ASTIdentifier& iden = *static_cast<ASTIdentifier*>(&node);
                iden.determineResultType(scopeStack);
            }
        }

        // infer expression result type
        pHead->type = pHead->inferType(scopeStack);
    } catch (TException& e) {
        // free & rethrow
        delete pHead;
        throw e;
    }

    return pHead;
}

ASTNode* parseConditional(const std::vector<Token>& tokens, const std::vector<size_t>& branchIndices, const size_t globalEndIndex, scope_stack_t& scopeStack) {
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
                    static_cast<ASTIfCondition*>(pNode)->pExpr = parseExpression(tokens, startIndex+2, openBrace-2, scopeStack);
                } else {
                    static_cast<ASTElseIfCondition*>(pNode)->pExpr = parseExpression(tokens, startIndex+2, openBrace-2, scopeStack);
                }

                // offset the body token position
                bodyStart = openBrace+1;
            } else {
                pNode = new ASTElseCondition(startToken);
                pHead->push( pNode );
            }

            // parse the body (up to but not including closing brace)
            parseBody(pNode, tokens, bodyStart, endIndex-1, scopeStack);
        }
    } catch (TException& e) {
        // free & rethrow
        delete pHead;
        throw e;
    }

    return pHead;
}

ASTNode* parseWhileLoop(const std::vector<Token>& tokens, const size_t startIndex, const size_t endIndex, scope_stack_t& scopeStack) {
    // create new node
    ASTWhileLoop* pHead = new ASTWhileLoop(tokens[startIndex]);

    // create a new scope
    scopeStack.push_back( parser_scope_t() );

    try {
        // append expression
        size_t i = startIndex;

        // find LBRACE
        do { ++i; } while (tokens[i].type != TokenType::LBRACE);

        // parse expression
        pHead->pExpr = parseExpression( tokens, startIndex+2, i-2, scopeStack ); // ignore parenthesis

        // parse body
        parseBody( pHead, tokens, i+1, endIndex-1, scopeStack ); // ignore braces
    } catch (TException& e) {
        delete pHead;
        throw e;
    }

    // pop this scope off the stack
    scopeStack.pop_back();

    // return node
    return pHead;
}

ASTNode* parseForLoop(const std::vector<Token>& tokens, const size_t startIndex, const size_t endIndex, scope_stack_t& scopeStack) {
    // create new node
    ASTForLoop* pHead = new ASTForLoop(tokens[startIndex]);

    // create a new scope
    scopeStack.push_back( parser_scope_t() );

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
        pHead->pExprA = parseExpression( tokens, startIndex+2, semiA-1, scopeStack); // ignore opening parenthesis & semicolon
        pHead->pExprB = parseExpression( tokens, semiA+1, semiB-1, scopeStack); // ignore parenthesis & semicolon
        pHead->pExprC = parseExpression( tokens, semiB+1, i-2, scopeStack); // ignore closing parenthesis

        // parse body
        parseBody( pHead, tokens, i+1, endIndex-1, scopeStack ); // ignore braces
    } catch (TException& e) {
        delete pHead;
        throw e;
    }

    // pop this scope off the stack
    scopeStack.pop_back();

    // return node
    return pHead;
}