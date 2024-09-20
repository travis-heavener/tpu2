#ifndef __TOKEN_HPP
#define __TOKEN_HPP

#include <string>

#include "err_info.hpp"

// token types
enum TokenType {
    RETURN, SEMICOLON, IDENTIFIER, IF, ELSE_IF, ELSE, WHILE, FOR,
    LPAREN, RPAREN, LBRACKET, RBRACKET, LBRACE, RBRACE, // (), [], {}
    TYPE_INT, TYPE_FLOAT, TYPE_CHAR, TYPE_BOOL, // type names
    LIT_INT, LIT_FLOAT, LIT_BOOL, LIT_CHAR, LIT_STRING, VOID, // type literals
    BLOCK_COMMENT_START, BLOCK_COMMENT_END,
    COMMA,

    // operators
    OP_LT, OP_LTE, OP_GT, OP_GTE, // <, <=, >, >=
    OP_LSHIFT, OP_RSHIFT, // <<, >>
    OP_ADD, OP_SUB, ASTERISK, OP_DIV, OP_MOD, // +, -, *, /, %
    OP_BIT_OR, AMPERSAND, OP_BIT_NOT, OP_BIT_XOR, // |, &, ~, ^
    OP_BOOL_OR, OP_BOOL_AND, OP_BOOL_NOT, // ||, &&, !
    OP_EQ, OP_NEQ, // ==, !=
    SIZEOF,

    // assignment operators
    ASSIGN
};

// Token class for for lexer
class Token {
    public:
        Token(ErrInfo err, const std::string& raw, TokenType type): err(err), raw(raw), type(type) {};
        Token(ErrInfo err, char raw, TokenType type): err(err), raw({raw}), type(type) {};
        ErrInfo err;
        const std::string raw;
        TokenType type;
};

// returns true if the given TokenType is that of a type name (ex. TYPE_INT)
bool isTokenTypeName(TokenType);

// token helpers
bool isTokenPrimitiveType(const TokenType);
bool isTokenUnaryOp(const TokenType);
bool isTokenBinaryOp(const TokenType);
bool isTokenLiteral(const TokenType);
bool isTokenCompOp(const TokenType);
bool isTokenAssignOp(const TokenType);
bool isNonValueType(const TokenType);

// returns the size of a primitive type in bytes
unsigned char getSizeOfType(TokenType type);

#endif