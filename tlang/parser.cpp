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
            do { i++; } while ( i < tokensLen && tokens[i].type != TokenType::RPAREN );

            if ( i == tokensLen ) throw TUnclosedGroupException(tokens[startIndex+2].err);

            // verify opening brace is present
            if ( i+1 == tokensLen || tokens[++i].type != TokenType::LBRACE )
                throw TInvalidTokenException(tokens[i].err);
            
            // verify closing brace is present
            endIndex = i; // store brace start in endIndex for now <--------------
            do { i++; } while ( i < tokensLen && tokens[i].type != TokenType::RBRACE );

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
    std::cout << tokens[startIndex].raw << ' ' << tokens[endIndex].raw << '\n';
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
        for ((void)i; i < endIndex; i++) {
            switch (tokens[i].type) {
                case TokenType::IF: { // parse conditional
                    break;
                }
                case TokenType::FOR: { // parse for-loop
                    break;
                }
                case TokenType::WHILE: { // parse while-loop
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