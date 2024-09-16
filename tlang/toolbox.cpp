#include <stdexcept>
#include <string>
#include <sys/stat.h>

#include "toolbox.hpp"
#include "t_exception.hpp"

// get the total size taken up on the stack by this type
size_t Type::getStackSizeBytes() const {
    size_t size = getSizeOfType(this->primitiveType);

    for (long long modifier : this->arraySizes) {
        // not specified
        if (modifier == TYPE_NO_ARR_SIZE)
            size *= TYPE_NO_ARR_SIZE < 0 ? -1 : 1;
        size *= modifier;
    }

    return size;
}

bool Type::hasEmptyArrayModifiers() const {
    for (long long modifier : this->arraySizes)
        if (modifier == TYPE_NO_ARR_SIZE)
            return true;

    // base case
    return false;
}

bool Type::operator==(const Type& t) const {
    // check primitive
    if (primitiveType != t.primitiveType)
        return false;

    // check sizes
    if (arraySizes.size() != t.arraySizes.size())
        return false;
    
    // check each modifier
    for (size_t i = 0; i < arraySizes.size(); ++i)
        if (arraySizes[i] != t.arraySizes[i])
            return false;
    
    // base case, match
    return true;
}

bool Type::checkArrayMods(const Type& t) const {
    // check sizes
    if (arraySizes.size() != t.arraySizes.size())
        return false;
    
    // check each modifier
    for (size_t i = 0; i < arraySizes.size(); ++i)
        if (arraySizes[i] != t.arraySizes[i])
            return false;
    
    // base case, match
    return true;
}

// it works out that by just checking the size of the primitive, we can determine
// which type to take. char & bool may be cast as int, and any may be cast as a double
Type Type::checkDominant(Type B) const {
    TokenType primA = this->primitiveType;
    TokenType primB = B.primitiveType;

    if (getSizeOfType(primA) > getSizeOfType(primB))
        return *this;

    // base case, assume B
    return B;
}

// modified from https://stackoverflow.com/a/6296808
bool doesFileExist(const std::string& file) {
    struct stat buf;
    return stat(file.c_str(), &buf) != -1;
}

// true if a character is valid in an identifier
bool isCharValidIdentifier(const char c) {
    return std::isalpha(c) || std::isdigit(c) || c == '_' || 'c' == '$';
}

// true if a character is valid at the start of an identifier
bool isCharValidIdentifierStart(const char c) {
    return isCharValidIdentifier(c) && !std::isdigit(c);
}

// used to expand an escaped character string
char escapeChar(const std::string& str) {
    if (str.size() == 1) return str[0];

    switch (str[1]) {
        case '\'': return '\'';
        case '"': return '"';
        case '\\': return '\\';
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'v': return '\v';
        case '0':
        default: return '\0';
    }
}

// returns true if the given TokenType is that of a type name (ex. TYPE_INT)
bool isTokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::TYPE_INT:
        case TokenType::TYPE_FLOAT:
        case TokenType::TYPE_CHAR:
        case TokenType::TYPE_BOOL:
        case TokenType::VOID:
            return true;
        default:
            return false;
    }
}

// true if the token is of TYPE_INT, TYPE_BOOL, etc.
bool isTokenPrimitiveType(const TokenType type) {
    return type == TYPE_BOOL || type == TYPE_CHAR || type == TYPE_FLOAT || type == TYPE_INT;
}

// true if the token is an unary operator (ex. ~, !)
bool isTokenUnaryOp(const TokenType type) {
    return type == OP_BOOL_NOT || type == OP_ADD || type == OP_SUB || type == OP_BIT_NOT;
}

// true if the token is a binary operator (ex. *, /, ==)
bool isTokenBinaryOp(const TokenType type) {
    return type == OP_LT || type == OP_LTE || type == OP_GT || type == OP_GTE ||
           type == OP_LSHIFT || type == OP_RSHIFT || type == OP_ADD ||
           type == OP_SUB || type == OP_MUL || type == OP_DIV || type == OP_MOD ||
           type == OP_BIT_OR || type == OP_BIT_AND || type == OP_BIT_XOR ||
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
           type == OP_BIT_AND || type == OP_BIT_XOR || type == OP_BOOL_OR || type == OP_BOOL_AND ||
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
        case TokenType::TYPE_FLOAT: return 4; // 4-byte floats
        case TokenType::TYPE_CHAR: return 1;
        case TokenType::TYPE_BOOL: return 1;
        case TokenType::VOID: return 0;
        default: throw std::invalid_argument("Invalid type passed to getSizeOfType.");
    }
}

// lookup variable from scope stack
Type lookupParserVariable(scope_stack_t& scopeStack, const std::string& name, ErrInfo err) {
    // look in the stack, up
    auto itr = scopeStack.rbegin();
    for ((void)itr; itr != scopeStack.rend(); itr++) {
        // check this scope
        parser_scope_t& parserScope = *itr;
        if (parserScope.count(name) > 0) {
            return parserScope.at(name);
        }
    }

    // base case, not found
    throw TUnknownIdentifierException(err);
}

// declare a variable in the immediate scope
void declareParserVariable(scope_stack_t& scopeStack, const std::string& name, Type type, ErrInfo err) {
    // verify this variable isn't already defined in the immediate scope
    parser_scope_t& parserScope = *scopeStack.rbegin();
    if (parserScope.count(name) > 0)
        throw TIdentifierInUseException(err);
    
    // declare variable
    parserScope[name] = type;
}