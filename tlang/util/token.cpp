#include <stdexcept>

#include "token.hpp"

// true if the token is of TYPE_INT, TYPE_BOOL, etc.
bool isTokenPrimitiveType(const TokenType type, const bool allowVoid) {
    return type == TYPE_BOOL || type == TYPE_CHAR || type == TYPE_FLOAT || type == TYPE_INT || (type == VOID && allowVoid);
}

// true if the token is an unary operator (ex. ~, !)
bool isTokenUnaryOp(const TokenType type) {
    return type == OP_BOOL_NOT || type == OP_ADD || type == OP_SUB || type == OP_BIT_NOT || type == SIZEOF;
}

// true if the token is a binary operator (ex. *, /, ==)
bool isTokenBinaryOp(const TokenType type) {
    return type == OP_LT || type == OP_LTE || type == OP_GT || type == OP_GTE ||
           type == OP_LSHIFT || type == OP_RSHIFT || type == OP_ADD ||
           type == OP_SUB || type == ASTERISK || type == OP_DIV || type == OP_MOD ||
           type == OP_BIT_OR || type == AMPERSAND || type == OP_BIT_XOR ||
           type == OP_BOOL_OR || type == OP_BOOL_AND || type == OP_EQ || type == OP_NEQ ||
           type == ASSIGN;
}

// true if the token is a literal
bool isTokenLiteral(const TokenType type) {
    return type == LIT_BOOL || type == LIT_CHAR || type == LIT_FLOAT || type == LIT_INT || type == VOID;
}

// true if the token is a comparison operator
bool isTokenCompOp(const TokenType type) {
    return type == OP_LT || type == OP_LTE || type == OP_GT || type == OP_GTE ||  type == OP_BIT_OR ||
           type == AMPERSAND || type == OP_BIT_XOR || type == OP_BOOL_OR || type == OP_BOOL_AND ||
           type == OP_EQ || type == OP_NEQ;
}

// true if the token is an assignment operator
bool isTokenAssignOp(const TokenType type) {
    return type == ASSIGN;
}

// returns the size of a primitive type in bytes
unsigned char getSizeOfType(TokenType type) {
    switch (type) {
        case TokenType::TYPE_INT: return 2; // 2-byte ints
        case TokenType::TYPE_FLOAT: return 2; // 2-byte floats
        case TokenType::TYPE_CHAR: return 1;
        case TokenType::TYPE_BOOL: return 1;
        case TokenType::VOID: return 0;
        default: throw std::invalid_argument("Invalid type passed to getSizeOfType.");
    }
}