#include <vector>
#include <iostream>

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

ASTNode* parseFunction(const std::vector<Token>& tokens, size_t startIndex, size_t endIndex) {
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
        
        // parse body (up to closing brace)
        for (++i; i < endIndex; i++) {
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
                            } while (parensOpen > 0 && endCond < endIndex);

                            if (endCond == endIndex) throw TUnclosedGroupException(tokens[parenStart].err);
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
                        } while (bracesOpen > 0 && endCond < endIndex);

                        if (endCond == endIndex) throw TUnclosedGroupException(tokens[braceStart].err);

                        // check for else on the same line
                        if (endCond+1 == endIndex || (tokens[endCond+1].type != TokenType::ELSE_IF && tokens[endCond+1].type != TokenType::ELSE)) {
                            // break out of conditional
                            endCond--;
                            isInConditional = false;
                        }

                        // base case, recurse
                        ++endCond;
                    }

                    // parse conditional
                    parseConditional(tokens, branchIndices, endCond);
                    i = endCond; // jump to end of conditional
                    break;
                }
                case TokenType::FOR: { // parse for-loop
                    // TO-DO implement here
                    throw std::runtime_error("Unimplemented");
                    break;
                }
                case TokenType::WHILE: { // parse while-loop
                    // TO-DO implement here
                    throw std::runtime_error("Unimplemented");
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
                    } while (endExpr < endIndex && tokens[endExpr].type != TokenType::SEMICOLON);
                    
                    // verify semicolon is present
                    if (endExpr == endIndex) throw TInvalidTokenException(tokens[i].err);

                    // append expression to pReturn
                    pReturn->push( parseExpression(tokens, i+1, endExpr-1) );
                    i = endExpr;
                    break;
                }
                case TokenType::BLOCK_COMMENT_START: { // search for close
                    size_t j = i;
                    do {
                        j++;
                    } while (j < endIndex && tokens[j].type != TokenType::BLOCK_COMMENT_END);

                    // handle unclosed comments
                    if (j == endIndex) throw TUnclosedCommentException(tokens[i].err);

                    // jump to end of comment
                    i = j;
                    break;
                }
                case TokenType::SEMICOLON: break; // erroneous statement
                default: { // base case, parse as expression
                    // get expression
                    size_t endExpr = i;
                    while (endExpr < endIndex && tokens[endExpr].type != TokenType::SEMICOLON)
                        ++endExpr;

                    // verify semicolon is present
                    if (endExpr == endIndex) throw TInvalidTokenException(tokens[i].err);
                    
                    pHead->push( parseExpression(tokens, i, endExpr-1) );
                    i = endExpr;
                    break;
                }
            }
        }
    } catch (TException& e) {
        delete pHead; // free & rethrow
        throw e;
    }

    // return node ptr
    return pHead;
}

ASTNode* parseExpression(const std::vector<Token>& tokens, size_t startIndex, size_t endIndex) {
    ASTNode* pHead = nullptr;
    
    std::cout << tokens[startIndex].raw << ' ' << tokens[endIndex].raw << '\n';

    // iterate through expression
    try {
        // TO-DO implement here
        throw std::runtime_error("Unimplemented");
    } catch (TException& e) {
        // free & rethrow
        if (pHead != nullptr) delete pHead;
        throw e;
    }

    return pHead;
}

ASTNode* parseConditional(const std::vector<Token>& tokens, const std::vector<size_t>& branchIndices, size_t globalEndIndex) {
    ASTNode* pHead = new ASTConditional(tokens[branchIndices[0]]);

    try {
        // iterate for each branch
        size_t numBranches = branchIndices.size();
        for (size_t i = 0; i < numBranches; i++) {
            // determine start and end indices
            size_t startIndex = branchIndices[i];
            size_t endIndex = i+1 == numBranches ? globalEndIndex : branchIndices[i+1]-1;
        }
    } catch (TException& e) {
        // free & rethrow
        if (pHead != nullptr) delete pHead;
        throw e;
    }

    return pHead;
}