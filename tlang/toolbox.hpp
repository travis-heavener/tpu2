#ifndef __TOOLBOX_HPP
#define __TOOLBOX_HPP

#include <string>

// helper to check if file exists
// modified from https://stackoverflow.com/a/6296808
bool doesFileExist(const std::string& file);

// true if a character is valid in an identifier
bool isCharValidIdentifier(const char);

// true if a character is valid at the start of an identifier
bool isCharValidIdentifierStart(const char);

// for error handling
typedef unsigned long long line_t; // for line/col numbering

// store where a token is from in its original file
class ErrInfo {
    public:
        ErrInfo(line_t line, line_t col) : line(line), col(col) {};
        line_t line;
        line_t col;
};

// token types
enum TokenType {
    RETURN, SEMICOLON, IDENTIFIER, IF, ELSE_IF, ELSE, WHILE, FOR,
    LPAREN, RPAREN, LBRACKET, RBRACKET, LBRACE, RBRACE, // (), [], {}
    TYPE_INT, TYPE_DOUBLE, TYPE_CHAR, TYPE_BOOL, // type names
    LIT_INT, LIT_DOUBLE, LIT_BOOL, LIT_CHAR, LIT_VOID, // type literals
    BLOCK_COMMENT_START, BLOCK_COMMENT_END,
    COMMA,

    // operators
    OP_LT, OP_LTE, // <, <=
    OP_GT, OP_GTE, // >, >=
    OP_LSHIFT, OP_RSHIFT, // <<, >>
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, // +, -, *, /, %
    OP_BIT_OR, OP_BIT_AND, OP_BIT_NOT, OP_BIT_XOR, // |, &, ~, ^
    OP_BOOL_OR, OP_BOOL_AND, OP_BOOL_NOT, // ||, &&, !
    OP_EQ, OP_NEQ, // ==, !=

    // assignment operators
    ASSIGN
};

// Token class for for lexer
class Token {
    public:
        Token(ErrInfo err, std::string raw, TokenType type): err(err), raw(raw), type(type) {};
        Token(ErrInfo err, char raw, TokenType type): err(err), raw({raw}), type(type) {};
        ErrInfo err;
        const std::string raw;
        TokenType type;
};

#endif