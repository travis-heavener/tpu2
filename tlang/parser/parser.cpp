#include <map>
#include <stack>
#include <vector>

#include "parser.hpp"
#include "parser_precedences.hpp"

#include "../util/token.hpp"
#include "../util/toolbox.hpp"
#include "../util/t_exception.hpp"
#include "../util/scope_stack.hpp"
#include "../ast/ast.hpp"
#include "../ast/ast_nodes.hpp"

// takes in a vector of Tokens and assembles an AST for the document
AST* parseToAST(const std::vector<Token>& tokens) {
    // create the AST
    AST* pAST = new AST();

    // create a scope stack to store variables and functions' types
    scope_stack_t scopeStack = { new ParserScope() }; // global scope

    // fill in the AST
    try {
        // only allow functions in the global scope
        size_t tokensLen = tokens.size();
        for (size_t i = 0; i < tokensLen; i++) {
            // parse the next function
            size_t startIndex = i, endIndex = i;

            // verify return type is specified
            if (!isTokenTypeKeyword(tokens[i].type))
                throw TSyntaxException(tokens[i].err);

            // grab type
            Type type( TokenType::TYPE_INT ); // defaults to signed int

            // while we have a type keyword, modify the type
            size_t start = i;
            while (i < tokensLen && isTokenTypeKeyword(tokens[i].type)) {
                // check const if first char
                if (i == start && tokens[i].type == TokenType::CONST) {
                    type.setIsConst(true);
                } else if (isTokenSignedUnsigned(tokens[i].type)) {
                    type.setIsUnsigned(tokens[i].type == TokenType::UNSIGNED);
                } else if (isTokenPrimitiveType(tokens[i].type, true)) {
                    type.setPrimType(tokens[i].type);
                    ++i;
                    break; // primitive must be last
                } else {
                    // invalid token
                    throw TInvalidTokenException(tokens[i].err);
                }
                ++i;
            }

            // grab pointers
            while (i < tokensLen && tokens[i].type == TokenType::ASTERISK) {
                type.addEmptyPointer(); // add to type
                ++i;
            }
            
            // verify an identifier name is present
            if ( i == tokensLen || tokens[i].type != TokenType::IDENTIFIER )
                throw TInvalidTokenException(tokens[i == tokensLen ? i-1 : i].err);
            
            // set the start index to where the identifier name is
            startIndex = i++;

            // verify opening parenthesis is present
            if ( i == tokensLen || tokens[i].type != TokenType::LPAREN )
                throw TInvalidTokenException(tokens[i-1].err);

            // verify closing parenthesis is present
            endIndex = findClosingParen(tokens, i, tokensLen-1);
            i = endIndex;

            // verify opening brace is present
            if ( i+1 == tokensLen || tokens[++i].type != TokenType::LBRACE )
                throw TInvalidTokenException(tokens[i].err);
            
            // verify closing brace is present
            endIndex = findClosingBrace(tokens, i, tokensLen-1);
            i = endIndex;

            // all good to go
            endIndex = i;
            pAST->push( parseFunction(tokens, startIndex, endIndex, scopeStack, pAST, type) );
        }
    } catch (TException& e) {
        while (scopeStack.size() > 0) // free ParserVar pointers
            popScopeStack(scopeStack);
        delete pAST; // free AST
        throw e; // rethrow
    }

    // remove unused variables & such
    while (scopeStack.size() > 0)
        popScopeStack(scopeStack);

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
                    if (tokens[branchIndices.back()].type == TokenType::ELSE || // the current branch is an else (nothing allowed after)
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
                if (i+1 < endExpr) { // if there is an expression
                    pReturn->push( parseExpression(tokens, i+1, endExpr-1, scopeStack, true) );
                }
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
                if (isTokenTypeKeyword(tokens[i].type)) {
                    // grab type
                    Type type; // default to void

                    // while we have a type keyword, modify the type
                    size_t start = i;
                    while (i <= endIndex && isTokenTypeKeyword(tokens[i].type)) {
                        // check const if first char
                        if (i == start && tokens[i].type == TokenType::CONST) {
                            type.setIsConst(true);
                        } else if (isTokenSignedUnsigned(tokens[i].type)) {
                            type.setIsUnsigned(tokens[i].type == TokenType::UNSIGNED);
                        } else if (isTokenPrimitiveType(tokens[i].type, true)) {
                            type.setPrimType(tokens[i].type);
                            ++i;
                            break; // primitive must be last
                        } else {
                            // invalid token
                            throw TInvalidTokenException(tokens[i].err);
                        }
                        ++i;
                    }

                    // grab pointers
                    while (i <= endIndex && tokens[i].type == TokenType::ASTERISK) {
                        type.addEmptyPointer(); // add to type
                        ++i;
                    }

                    // verify not end of input
                    if (i > endIndex) throw TInvalidTokenException(tokens[i-1].err);

                    // only allow unsigned int or char, and disallow void non-ptrs
                    bool isInvalidUnsigned = type.isUnsigned() && type.getPrimType() != TokenType::TYPE_INT && type.getPrimType() != TokenType::TYPE_CHAR;
                    if (isInvalidUnsigned || type.isVoidNonPtr()) {
                        throw TSyntaxException(tokens[start].err);
                    }

                    // get identifier
                    if (tokens[i].type != TokenType::IDENTIFIER)
                        throw TInvalidTokenException(tokens[i].err);
                    size_t idenStart = i;

                    // get all array size hints (only allow integer sizes)
                    bool hasImplicitArraySizeHints = false;
                    if (tokens[++i].type == TokenType::LBRACKET) {
                        size_t j, numHints = 0;
                        for (j = i; j <= endIndex && tokens[j].type == TokenType::LBRACKET; (void)j) {
                            if (tokens[j+1].type == TokenType::LIT_INT || numHints > 0) {
                                // verify next token is an int literal
                                if (tokens[j+1].type != TokenType::LIT_INT)
                                    throw TInvalidTokenException(tokens[i+1].err);

                                // verify next token is an RBRACKET
                                if (tokens[j+2].type != TokenType::RBRACKET)
                                    throw TInvalidTokenException(tokens[j+2].err);

                                // add with value otherwise
                                type.addHintPointer( std::stol(tokens[j+1].raw) );
                                j += 3;
                            } else {
                                // verify next token is an RBRACKET
                                if (tokens[j+1].type != TokenType::RBRACKET)
                                    throw TInvalidTokenException(tokens[j+1].err);

                                // add empty array modifier if first bracket pair
                                type.addHintPointer( TYPE_EMPTY_PTR );
                                j += 2;
                                hasImplicitArraySizeHints = true;
                            }
                            numHints++;
                        }
                        i = j;
                    }

                    // create node
                    ASTVarDeclaration* pVarDec = new ASTVarDeclaration(tokens[start], type);
                    pHead->push(pVarDec); // append here so it gets freed on its own if an error occurs here

                    // create identifier node (always an assignment operator)
                    pVarDec->pIdentifier = new ASTIdentifier(tokens[idenStart], true);

                    // if declared but not assigned, leave pExpr as nullptr
                    if (tokens[i].type == TokenType::SEMICOLON) {
                        // verify that any subscripts for an array type are provided
                        if (hasImplicitArraySizeHints)
                            throw TIllegalArraySizeException(tokens[start].err);

                        // add variable to scopeStack
                        ParserVariable* pParserVar = new ParserVariable(type, pHead, pVarDec);
                        declareParserVariable(scopeStack, tokens[idenStart].raw, pParserVar, tokens[idenStart].err);
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
                    ASTExpr* pExpr = static_cast<ASTExpr*>(parseExpression(tokens, i+1, endExpr-1, scopeStack, true));
                    pVarDec->pExpr = pExpr;
                    i = endExpr; // update `i` to position of semicolon

                    // confirm assignment type matches
                    if (hasImplicitArraySizeHints) {
                        // convert to string literal
                        if (type.getNumPointers() == 1 && pExpr->at(0)->getNodeType() == ASTNodeType::LIT_STRING) {
                            pVarDec->updateType( type );
                            type = pVarDec->type;
                        }

                        // imply size from literal's type
                        if (pExpr->at(0)->getNodeType() != ASTNodeType::LIT_ARR)
                            throw TSyntaxException(pExpr->at(0)->err);

                        // update implied size
                        ASTArrayLiteral* pArrLit = static_cast<ASTArrayLiteral*>(pExpr->at(0));
                        type.setArrayHint(type.getNumArrayHints()-1, pArrLit->size());
                    }

                    // update variable declaration's type
                    pVarDec->updateType( type );
                    type = pVarDec->type;

                    // add variable to scopeStack
                    ParserVariable* pParserVar = new ParserVariable(type, pHead, pVarDec);
                    declareParserVariable(scopeStack, tokens[idenStart].raw, pParserVar, tokens[idenStart].err);
                    break;
                }

                // base case, parse as an expression
                size_t endExpr = i;
                while (endExpr <= endIndex && tokens[endExpr].type != TokenType::SEMICOLON)
                    ++endExpr;

                // verify semicolon is present
                if (endExpr > endIndex) throw TInvalidTokenException(tokens[i].err);
                pHead->push( parseExpression(tokens, i, endExpr-1, scopeStack, true) );
                i = endExpr;
                break;
            }
        }
    }
}

ASTNode* parseFunction(const std::vector<Token>& tokens, const size_t startIndex, const size_t endIndex, scope_stack_t& scopeStack, AST* pAST, Type retType) {
    size_t i = startIndex;

    // create node
    const std::string name = tokens[i].raw; // get function name
    ASTFunction* pHead = new ASTFunction(name, tokens[startIndex], retType);
    scopeStack.push_back( new ParserScope() ); // create a new scope stack for scoped parser variables

    try {
        i += 2; // skip over function name & opening parenthesis

        // append parameters
        while (tokens[i].type != TokenType::RPAREN) {
            // grab type
            Type type; // default to void

            // while we have a type keyword, modify the type
            size_t start = i;
            while (i <= endIndex && isTokenTypeKeyword(tokens[i].type)) {
                // check const if first char
                if (i == start && tokens[i].type == TokenType::CONST) {
                    type.setIsConst(true);
                } else if (isTokenSignedUnsigned(tokens[i].type)) {
                    type.setIsUnsigned(tokens[i].type == TokenType::UNSIGNED);
                } else if (isTokenPrimitiveType(tokens[i].type, true)) {
                    type.setPrimType(tokens[i].type);
                    ++i;
                    break; // primitive must be last
                } else {
                    // invalid token
                    throw TInvalidTokenException(tokens[i].err);
                }
                ++i;
            }

            // grab pointers
            while (i <= endIndex && tokens[i].type == TokenType::ASTERISK) {
                type.addEmptyPointer(); // add to type
                ++i;
            }

            // verify not end of input
            if (i > endIndex) throw TInvalidTokenException(tokens[i-1].err);

            // get identifier
            if (tokens[i].type != TokenType::IDENTIFIER)
                throw TInvalidTokenException(tokens[i].err);
            size_t idenStart = i;

            // get all array size hints (only allow integer sizes)
            if (tokens[++i].type == TokenType::LBRACKET) {
                size_t j, numHints = 0;
                for (j = i; j <= endIndex && tokens[j].type == TokenType::LBRACKET; (void)j) {
                    if (tokens[j+1].type == TokenType::LIT_INT || numHints > 0) {
                        // verify next token is an int literal
                        if (tokens[j+1].type != TokenType::LIT_INT)
                            throw TInvalidTokenException(tokens[i+1].err);

                        // verify next token is an RBRACKET
                        if (tokens[j+2].type != TokenType::RBRACKET)
                            throw TInvalidTokenException(tokens[j+2].err);

                        // add with value otherwise
                        type.addHintPointer( std::stol(tokens[j+1].raw) );
                        j += 3;
                    } else {
                        // verify next token is an RBRACKET
                        if (tokens[j+1].type != TokenType::RBRACKET)
                            throw TInvalidTokenException(tokens[j+1].err);

                        // add empty array modifier if first bracket pair
                        type.addHintPointer( TYPE_EMPTY_PTR );
                        j += 2;
                    }
                    numHints++;
                }
                i = j;
            }

            // only allow unsigned int or char, and disallow void non-ptrs
            bool isInvalidUnsigned = type.isUnsigned() && type.getPrimType() != TokenType::TYPE_INT && type.getPrimType() != TokenType::TYPE_CHAR;
            if (isInvalidUnsigned || type.isVoidNonPtr()) {
                throw TSyntaxException(tokens[start].err);
            }

            // grab parameter name
            if (tokens[idenStart].type != TokenType::IDENTIFIER)
                throw TInvalidTokenException(tokens[idenStart].err);

            const std::string paramName = tokens[idenStart].raw;
            ASTFuncParam* pParam = new ASTFuncParam(paramName, type);
            pHead->appendParam(pParam); // append parameter

            // add argument to scopeStack
            ParserVariable* pParserVar = new ParserVariable(type);
            declareParserVariable(scopeStack, tokens[idenStart].raw, pParserVar, tokens[idenStart].err);

            if (tokens[i].type == TokenType::COMMA) i++; // skip next comma
        }

        // determine if this is a main function
        bool isMainFunction = pHead->isMainFunction();

        // declare function in parent scope
        std::vector<Type> paramTypes;
        pHead->loadParamTypes(paramTypes);

        ParserFunction* pParserFunc = new ParserFunction(retType, isMainFunction, pAST, pHead, paramTypes);
        declareParserFunction(scopeStack, name, pParserFunc, paramTypes, tokens[startIndex].err); // put into scope

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

ASTNode* parseExpression(const std::vector<Token>& tokens, const size_t startIndex, const size_t endIndex,
                         scope_stack_t& scopeStack, const bool isTopExpr) {
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

        // infer type recursively for all nodes
        pHead->inferType(scopeStack);
    } catch (TException& e) {
        // free & rethrow
        delete pHead;
        throw e;
    }

    // verify only one child remains if top expression
    if (isTopExpr && pHead->size() != 1)
        throw TExpressionEvalException(pHead->err);

    return pHead;
}

ASTNode* parseConditional(const std::vector<Token>& tokens, const std::vector<size_t>& branchIndices, const size_t globalEndIndex, scope_stack_t& scopeStack) {
    ASTNode* pHead = new ASTConditional(tokens[branchIndices[0]]);

    try {
        // iterate for each branch
        size_t numBranches = branchIndices.size();
        for (size_t i = 0; i < numBranches; i++) {
            // create a new scope
            scopeStack.push_back( new ParserScope() );

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
                    ASTNode* pExpr = parseExpression(tokens, startIndex+2, openBrace-2, scopeStack, true);
                    static_cast<ASTExpr*>(pExpr)->setType(Type(TokenType::TYPE_BOOL));
                    static_cast<ASTIfCondition*>(pNode)->pExpr = pExpr;
                } else {
                    ASTNode* pExpr = parseExpression(tokens, startIndex+2, openBrace-2, scopeStack, true);
                    static_cast<ASTExpr*>(pExpr)->setType(Type(TokenType::TYPE_BOOL));
                    static_cast<ASTElseIfCondition*>(pNode)->pExpr = pExpr;
                }

                // offset the body token position
                bodyStart = openBrace+1;
            } else {
                pNode = new ASTElseCondition(startToken);
                pHead->push( pNode );
            }

            // parse the body (up to but not including closing brace)
            parseBody(pNode, tokens, bodyStart, endIndex-1, scopeStack);

            // pop this scope off the stack
            popScopeStack(scopeStack);
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
    scopeStack.push_back( new ParserScope() );

    try {
        // append expression
        size_t i = startIndex;

        // find LBRACE
        do { ++i; } while (tokens[i].type != TokenType::LBRACE);

        // parse expression
        pHead->pExpr = parseExpression( tokens, startIndex+2, i-2, scopeStack, true ); // ignore parenthesis

        // force as bool
        static_cast<ASTExpr*>(pHead->pExpr)->setType(Type(TokenType::TYPE_BOOL));

        // parse body
        parseBody( pHead, tokens, i+1, endIndex-1, scopeStack ); // ignore braces
    } catch (TException& e) {
        delete pHead;
        throw e;
    }

    // pop this scope off the stack
    popScopeStack(scopeStack);

    // return node
    return pHead;
}

ASTNode* parseForLoop(const std::vector<Token>& tokens, const size_t startIndex, const size_t endIndex, scope_stack_t& scopeStack) {
    // create new node
    ASTForLoop* pHead = new ASTForLoop(tokens[startIndex]);

    // create a new scope
    scopeStack.push_back( new ParserScope() );

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
        pHead->pExprA = parseExpression( tokens, startIndex+2, semiA-1, scopeStack, true); // ignore opening parenthesis & semicolon
        pHead->pExprB = parseExpression( tokens, semiA+1, semiB-1, scopeStack, true); // ignore parenthesis & semicolon
        pHead->pExprC = parseExpression( tokens, semiB+1, i-2, scopeStack, true); // ignore closing parenthesis

        // force as bool
        static_cast<ASTExpr*>(pHead->pExprA)->setType(Type(TokenType::TYPE_BOOL));
        static_cast<ASTExpr*>(pHead->pExprB)->setType(Type(TokenType::TYPE_BOOL));
        static_cast<ASTExpr*>(pHead->pExprC)->setType(Type(TokenType::TYPE_BOOL));

        // parse body
        parseBody( pHead, tokens, i+1, endIndex-1, scopeStack ); // ignore braces
    } catch (TException& e) {
        delete pHead;
        throw e;
    }

    // pop this scope off the stack
    popScopeStack(scopeStack);

    // return node
    return pHead;
}