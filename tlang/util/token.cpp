#include <stdexcept>

#include "token.hpp"

// true if the token is of TYPE_INT, TYPE_BOOL, etc.
bool isTokenPrimitiveType(const TokenType type, const bool allowVoid) {
    return type == TYPE_BOOL || type == TYPE_CHAR || type == TYPE_FLOAT || type == TYPE_INT || (type == VOID && allowVoid);
}

// true if the token is UNSIGNED or constexpr SIGNED
bool isTokenSignedUnsigned(const TokenType type) {
    return type == UNSIGNED || type == SIGNED;
}

// true if the token is UNSIGNED/SIGNED, CONST, or a PRIMITIVE TYPE
bool isTokenTypeKeyword(const TokenType type) {
    return type == TYPE_BOOL || type == TYPE_CHAR || type == TYPE_FLOAT || type == TYPE_INT || type == VOID || type == UNSIGNED || type == SIGNED || type == CONST || type == STRUCT;
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
           isTokenAssignOp(type);
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
    return type == ASSIGN || type == OP_ADD_EQ || type == OP_SUB_EQ || type == OP_MUL_EQ || type == OP_DIV_EQ || type == OP_MOD_EQ || type == OP_LSHIFT_EQ || type == OP_RSHIFT_EQ || type == OP_BIT_AND_EQ || type == OP_BIT_OR_EQ || type == OP_BIT_XOR_EQ;
}

bool isTokenProtectedASM(const TokenType type) {
    return type == ASM_LOAD_AX || type == ASM_LOAD_BX || type == ASM_LOAD_CX || type == ASM_LOAD_DX ||
           type == ASM_READ_AX || type == ASM_READ_BX || type == ASM_READ_CX || type == ASM_READ_DX;
}

Token reduceAssignOpToken(const Token& refToken, const TokenType type) {
    switch (type) {
        case OP_ADD_EQ:     return Token(refToken.err, "+=",  OP_ADD);
        case OP_SUB_EQ:     return Token(refToken.err, "-=",  OP_SUB);
        case OP_MUL_EQ:     return Token(refToken.err, "*=",  ASTERISK);
        case OP_DIV_EQ:     return Token(refToken.err, "/=",  OP_DIV);
        case OP_MOD_EQ:     return Token(refToken.err, "%=",  OP_MOD);
        case OP_LSHIFT_EQ:  return Token(refToken.err, "<<=", OP_LSHIFT);
        case OP_RSHIFT_EQ:  return Token(refToken.err, ">>=", OP_RSHIFT);
        case OP_BIT_AND_EQ: return Token(refToken.err, "&=",  AMPERSAND);
        case OP_BIT_OR_EQ:  return Token(refToken.err, "|=",  OP_BIT_OR);
        case OP_BIT_XOR_EQ: return Token(refToken.err, "^=",  OP_BIT_XOR);
        default: throw TDevException("Invalid token type in reduceAssignOpToken" + std::to_string(type));
    }
}

// returns the size of a primitive type in bytes
unsigned char getSizeOfType(TokenType type) {
    switch (type) {
        case TokenType::TYPE_INT: return 2; // 2-byte ints
        case TokenType::TYPE_FLOAT: return 2; // 2-byte floats
        case TokenType::TYPE_CHAR: return 1;
        case TokenType::TYPE_BOOL: return 1;
        case TokenType::VOID: return 0;
        default: throw TDevException("Invalid type passed to getSizeOfType.");
    }
}