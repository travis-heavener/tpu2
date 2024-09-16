#ifndef __TOOLBOX_HPP
#define __TOOLBOX_HPP

#include <algorithm>
#include <map>
#include <string>
#include <vector>

// token types
enum TokenType {
    RETURN, SEMICOLON, IDENTIFIER, IF, ELSE_IF, ELSE, WHILE, FOR,
    LPAREN, RPAREN, LBRACKET, RBRACKET, LBRACE, RBRACE, // (), [], {}
    TYPE_INT, TYPE_FLOAT, TYPE_CHAR, TYPE_BOOL, // type names
    LIT_INT, LIT_FLOAT, LIT_BOOL, LIT_CHAR, VOID, // type literals
    BLOCK_COMMENT_START, BLOCK_COMMENT_END,
    COMMA,

    // operators
    OP_LT, OP_LTE, OP_GT, OP_GTE, // <, <=, >, >=
    OP_LSHIFT, OP_RSHIFT, // <<, >>
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, // +, -, *, /, %
    OP_BIT_OR, OP_BIT_AND, OP_BIT_NOT, OP_BIT_XOR, // |, &, ~, ^
    OP_BOOL_OR, OP_BOOL_AND, OP_BOOL_NOT, // ||, &&, !
    OP_EQ, OP_NEQ, // ==, !=

    // assignment operators
    ASSIGN
};

// literal types
#define TYPE_NO_ARR_SIZE -1
class Type {
    public:
        Type() : primitiveType(TokenType::VOID), arraySizes() {};
        Type(TokenType primitiveType) : primitiveType(primitiveType), arraySizes() {};

        // for adding an array with a specified size (ie. declarations)
        void addArrayModifier(size_t size) { arraySizes.push_back(size); };

        // for adding an array without a specified size (ie. parameter type)
        void addEmptyArrayModifier() { arraySizes.push_back(TYPE_NO_ARR_SIZE); };
        bool hasEmptyArrayModifiers() const;
        void popArrayModifier() { arraySizes.pop_back(); };

        bool isArray() const { return arraySizes.size() > 0; };

        Type checkDominant(Type B) const;

        const std::vector<long long>& getArrayModifiers() const { return arraySizes; };
        size_t getStackSizeBytes() const;

        TokenType getPrimitiveType() const { return primitiveType; };

        bool operator==(const Type&) const;
        bool operator!=(const Type& t) const { return !(*this == t); };
        bool checkArrayMods(const Type& t) const;
        void flipModifiers() { std::reverse(arraySizes.begin(), arraySizes.end()); };
    private:
        TokenType primitiveType;
        std::vector<long long> arraySizes;
};

// helper to check if file exists
// modified from https://stackoverflow.com/a/6296808
bool doesFileExist(const std::string& file);

// true if a character is valid in an identifier
bool isCharValidIdentifier(const char);

// true if a character is valid at the start of an identifier
bool isCharValidIdentifierStart(const char);

// used to expand an escaped character string
char escapeChar(const std::string&);

// returns true if the given TokenType is that of a type name (ex. TYPE_INT)
bool isTokenTypeName(TokenType);

// token helpers
bool isTokenPrimitiveType(const TokenType);
bool isTokenUnaryOp(const TokenType);
bool isTokenBinaryOp(const TokenType);
bool isTokenLiteral(const TokenType);
bool isTokenCompOp(const TokenType);
bool isTokenAssignOp(const TokenType);

// returns the size of a primitive type in bytes
unsigned char getSizeOfType(TokenType type);

// for error handling
typedef unsigned long long line_t; // for line/col numbering

// store where a token is from in its original file
class ErrInfo {
    public:
        ErrInfo(line_t line, line_t col) : line(line), col(col) {};
        line_t line, col;
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

// for parser scope stacks
typedef std::map<std::string, Type> parser_scope_t;
typedef std::vector<parser_scope_t> scope_stack_t;

// lookup variable from scope stack
Type lookupParserVariable(scope_stack_t&, const std::string&, ErrInfo);

// declare a variable in the immediate scope
void declareParserVariable(scope_stack_t&, const std::string&, Type, ErrInfo);

#endif