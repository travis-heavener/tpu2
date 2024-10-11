#include <fstream>
#include <vector>

#include "lexer.hpp"
#include "preprocessor.hpp"

#include "util/toolbox.hpp"
#include "util/token.hpp"
#include "util/t_exception.hpp"

static bool isInMultilineComment = false;
static macrodef_map macrodefMap;

// returns true if the keyword is present and is not part of an identifier
bool isKwdPresent(const std::string& kwd, const std::string& line, size_t offset) {
    if (line.find(kwd, offset) != offset) return false; // not present

    // verify no identifier tacked onto the end
    const size_t end = offset + kwd.size();
    return end == line.size() || !isCharValidIdentifier(line[end]);
}

// tokenize a document to a vector of Tokens
void tokenize(std::ifstream& inHandle, std::vector<Token>& tokens, cwd_stack& cwdStack, const std::string& filename, const bool isStdlib) {
    // continuously read in the entire document, adding tokens
    std::string line;

    line_t lineNumber = 0;
    while (std::getline(inHandle, line)) {
        // remove trailing \r if present (CRLF for Windows systems)
        if (*line.rbegin() == '\r') line.pop_back();

        // tokenize line
        tokenizeLine(line, tokens, ++lineNumber, cwdStack, filename, isStdlib);
    }
}

// tokenize a particular line
void tokenizeLine(std::string& line, std::vector<Token>& tokens, line_t lineNumber, cwd_stack& cwdStack, const std::string& filename, const bool isStdlib) {
    if (line.size() == 0) return;

    // if in a multiline comment, look for closing char
    size_t i = 0;
    if (isInMultilineComment) {
        if ((i = line.find("*/")) == std::string::npos) {
            return;
        } else { // jump to closing comment
            i += 2;
            isInMultilineComment = false;
        }
    }

    // replace any macro definitions
    replaceMacrodefs(line, macrodefMap, i);

    // run preprocessor for this line
    if (!isInMultilineComment) {
        bool hasPreprocessed = preprocessLine(line, macrodefMap, tokens, cwdStack, ErrInfo(lineNumber, 0, filename));

        // skip lines used by the preprocessor
        if (hasPreprocessed) return;
    }

    // iterate over all characters
    const size_t lineLen = line.size();
    std::string buffer; // mutable buffer cleared each iteration
    for ((void)i; i < lineLen; i++) {
        if (std::isspace(line[i])) continue; // skip whitespace

        // reset buffer & prepare error info object
        buffer.clear();
        ErrInfo err(lineNumber, i+1, filename);

        // handle the current character
        // break on single line comments
        if (line.find("//", i) == i) break;

        // handle multiline comments
        if (line.find("/*", i) == i) {
            i++;
            // try to jump to closing comment
            size_t closeIndex;
            if ((closeIndex = line.find("*/", i)) != std::string::npos) {
                i = closeIndex+1;
                isInMultilineComment = false;
            } else {
                isInMultilineComment = true;
                return;
            }
            continue;
        }

        // int/float literals (cannot start with a decimal/period!!)
        if (std::isdigit(line[i])) {
            TokenType tokenType = TokenType::LIT_INT;
            buffer = line[i]; // update buffer
            while (++i < lineLen && (std::isdigit(line[i]) || line[i] == '.')) {
                if (line[i] == '.') tokenType = TokenType::LIT_FLOAT; // set as double if encountering a decimal
                buffer.push_back(line[i]);
            }

            // rollback iterator if not at end
            if (i != lineLen) i--;

            // add the token
            tokens.push_back(Token(err, buffer, tokenType));
            continue;
        }
        
        // character literals
        if (line[i] == '\'') {
            // look for closing quote
            buffer = line[i];
            while (++i < lineLen) {
                buffer.push_back(line[i]); // add character
                if (line[i] == '\\') { // check for escape characters and skip next character
                    if (i+1 == lineLen) throw TInvalidEscapeException(err);
                    buffer.push_back(line[++i]); // add escaped character
                } else if (line[i] == '\'') { // break on closing quote
                    break;
                }
            }

            // if reached the end of the line, unclosed quote (don't rollback, breaks when hitting quote)
            if (i == lineLen) throw TUnclosedQuoteException(err);
            
            // otherwise, add the token
            tokens.push_back(Token(err, buffer, TokenType::LIT_CHAR));
            continue;
        }

        // string literals
        if (line[i] == '"') {
            // look for closing quote
            buffer = line[i];
            while (++i < lineLen) {
                buffer.push_back(line[i]); // add character
                if (line[i] == '\\') { // check for escape characters and skip next character
                    if (i+1 == lineLen) throw TInvalidEscapeException(err);
                    buffer.push_back(line[++i]); // add escaped character
                } else if (line[i] == '"') { // break on closing quote
                    break;
                }
            }

            // if reached the end of the line, unclosed quote (don't rollback, breaks when hitting quote)
            if (i == lineLen) throw TUnclosedQuoteException(err);
            
            // otherwise, add the token
            tokens.push_back(Token(err, buffer, TokenType::LIT_STRING));
            continue;
        }
        
        // literal boolean
        if (isKwdPresent("true", line, i)) {
            i += 3; // offset by length of keyword - 1
            tokens.push_back(Token(err, "true", TokenType::LIT_BOOL));
            continue;
        } else if (isKwdPresent("false", line, i)) {
            i += 4; // offset by length of keyword - 1
            tokens.push_back(Token(err, "false", TokenType::LIT_BOOL));
            continue;
        }

        // literal void
        if (isKwdPresent("void", line, i)) {
            i += 3; // offset by length of keyword - 1
            tokens.push_back(Token(err, "void", TokenType::VOID));
            continue;
        }

        // sizeof keyword
        if (isKwdPresent("sizeof", line, i)) {
            i += 5; // offset by length of keyword - 1
            tokens.push_back(Token(err, "sizeof", TokenType::SIZEOF));
            continue;
        }
        
        // asm keyword
        if (isKwdPresent("asm", line, i)) {
            i += 2; // offset by length of keyword - 1
            tokens.push_back(Token(err, "asm", TokenType::ASM));
            continue;
        }

        // protected assembly keywords
        if (isKwdPresent("__load_AX", line, i)) {
            if (!isStdlib) throw TInvalidTokenException(err);
            i += 8; // offset by length of keyword - 1
            tokens.push_back(Token(err, "__load_AX", TokenType::ASM_LOAD_AX));
            continue;
        } else if (isKwdPresent("__load_BX", line, i)) {
            if (!isStdlib) throw TInvalidTokenException(err);
            i += 8; // offset by length of keyword - 1
            tokens.push_back(Token(err, "__load_BX", TokenType::ASM_LOAD_BX));
            continue;
        } else if (isKwdPresent("__load_CX", line, i)) {
            if (!isStdlib) throw TInvalidTokenException(err);
            i += 8; // offset by length of keyword - 1
            tokens.push_back(Token(err, "__load_CX", TokenType::ASM_LOAD_CX));
            continue;
        } else if (isKwdPresent("__load_DX", line, i)) {
            if (!isStdlib) throw TInvalidTokenException(err);
            i += 8; // offset by length of keyword - 1
            tokens.push_back(Token(err, "__load_DX", TokenType::ASM_LOAD_DX));
            continue;
        }

        // unsigned & signed keywords
        if (isKwdPresent("unsigned", line, i)) {
            i += 7; // offset by length of keyword - 1
            tokens.push_back(Token(err, "unsigned", TokenType::UNSIGNED));
            continue;
        } else if (isKwdPresent("signed", line, i)) {
            i += 5; // offset by length of keyword - 1
            tokens.push_back(Token(err, "signed", TokenType::SIGNED));
            continue;
        }

        // check for individual characters that can't be a part of a larger operator or anything
        #define ADD_SINGLE_CHAR_TOKEN(c, type) tokens.push_back(Token(err, c, type)); continue;
        switch (line[i]) {
            case '(': ADD_SINGLE_CHAR_TOKEN(line[i], TokenType::LPAREN)
            case ')': ADD_SINGLE_CHAR_TOKEN(line[i], TokenType::RPAREN)
            case '{': ADD_SINGLE_CHAR_TOKEN(line[i], TokenType::LBRACE)
            case '}': ADD_SINGLE_CHAR_TOKEN(line[i], TokenType::RBRACE)
            case '[': ADD_SINGLE_CHAR_TOKEN(line[i], TokenType::LBRACKET)
            case ']': ADD_SINGLE_CHAR_TOKEN(line[i], TokenType::RBRACKET)
            case ';': ADD_SINGLE_CHAR_TOKEN(line[i], TokenType::SEMICOLON)
            case ',': ADD_SINGLE_CHAR_TOKEN(line[i], TokenType::COMMA)
            case '~': ADD_SINGLE_CHAR_TOKEN(line[i], TokenType::OP_BIT_NOT)
            case '^': ADD_SINGLE_CHAR_TOKEN(line[i], TokenType::OP_BIT_XOR)
            case '+': ADD_SINGLE_CHAR_TOKEN(line[i], TokenType::OP_ADD)
            case '-': ADD_SINGLE_CHAR_TOKEN(line[i], TokenType::OP_SUB)
            case '*': ADD_SINGLE_CHAR_TOKEN(line[i], TokenType::ASTERISK)
            case '/': ADD_SINGLE_CHAR_TOKEN(line[i], TokenType::OP_DIV)
            case '%': ADD_SINGLE_CHAR_TOKEN(line[i], TokenType::OP_MOD)
        }

        // check for keywords
        if (isKwdPresent("return", line, i)) {
            tokens.push_back(Token(err, "return", TokenType::RETURN));
            i += 5; // offset by length of keyword - 1
            continue;
        } else if (isKwdPresent("if", line, i)) {
            tokens.push_back(Token(err, "if", TokenType::IF));
            i += 1; // offset by length of keyword - 1
            continue;
        } else if (isKwdPresent("else if", line, i)) {
            tokens.push_back(Token(err, "else if", TokenType::ELSE_IF));
            i += 6; // offset by length of keyword - 1
            continue;
        } else if (isKwdPresent("else", line, i)) {
            tokens.push_back(Token(err, "else", TokenType::ELSE));
            i += 3; // offset by length of keyword - 1
            continue;
        } else if (isKwdPresent("while", line, i)) {
            tokens.push_back(Token(err, "while", TokenType::WHILE));
            i += 4; // offset by length of keyword - 1
            continue;
        } else if (isKwdPresent("for", line, i)) {
            tokens.push_back(Token(err, "for", TokenType::FOR));
            i += 2; // offset by length of keyword - 1
            continue;
        } else if (isKwdPresent("int", line, i)) {
            tokens.push_back(Token(err, "int", TokenType::TYPE_INT));
            i += 2; // offset by length of keyword - 1
            continue;
        } else if (isKwdPresent("double", line, i)) {
            tokens.push_back(Token(err, "double", TokenType::TYPE_FLOAT));
            i += 5; // offset by length of keyword - 1
            continue;
        } else if (isKwdPresent("char", line, i)) {
            tokens.push_back(Token(err, "char", TokenType::TYPE_CHAR));
            i += 3; // offset by length of keyword - 1
            continue;
        } else if (isKwdPresent("bool", line, i)) {
            tokens.push_back(Token(err, "bool", TokenType::TYPE_BOOL));
            i += 3; // offset by length of keyword - 1
            continue;
        } else if (isCharValidIdentifierStart(line[i])) {
            // handle identifier
            buffer = line[i];
            while (++i < lineLen && isCharValidIdentifier(line[i]))
                buffer.push_back(line[i]);

            // rollback iterator if not at end
            if (i != lineLen) i--;

            // add the token
            tokens.push_back(Token(err, buffer, TokenType::IDENTIFIER));
            continue;
        }

        // switch on more complex operators
        switch (line[i]) {
            case '<': {
                if (line.find("<<", i) == i) {
                    i++; // offset by length of keyword - 1
                    tokens.push_back(Token(err, "<<", TokenType::OP_LSHIFT));
                } else if (line.find("<=", i) == i) {
                    i++; // offset by length of keyword - 1
                    tokens.push_back(Token(err, "<=", TokenType::OP_LTE));
                } else {
                    tokens.push_back(Token(err, "<", TokenType::OP_LT));
                }
                continue;
            }
            case '>': {
                if (line.find(">>", i) == i) {
                    i++; // offset by length of keyword - 1
                    tokens.push_back(Token(err, ">>", TokenType::OP_RSHIFT));
                } else if (line.find(">=", i) == i) {
                    i++; // offset by length of keyword - 1
                    tokens.push_back(Token(err, ">=", TokenType::OP_GTE));
                } else {
                    tokens.push_back(Token(err, ">", TokenType::OP_GT));
                }
                continue;
            }
            case '&': {
                if (line.find("&&", i) == i) {
                    i++; // offset by length of keyword - 1
                    tokens.push_back(Token(err, "&&", TokenType::OP_BOOL_AND));
                } else {
                    tokens.push_back(Token(err, "&", TokenType::AMPERSAND));
                }
                continue;
            }
            case '|': {
                if (line.find("||", i) == i) {
                    i++; // offset by length of keyword - 1
                    tokens.push_back(Token(err, "||", TokenType::OP_BOOL_OR));
                } else {
                    tokens.push_back(Token(err, "|", TokenType::OP_BIT_OR));
                }
                continue;
            }
            case '!': {
                if (line.find("!=", i) == i) {
                    i++; // offset by length of keyword - 1
                    tokens.push_back(Token(err, "!=", TokenType::OP_NEQ));
                } else {
                    tokens.push_back(Token(err, "!", TokenType::OP_BOOL_NOT));
                }
                continue;
            }
            case '=': {
                if (line.find("==", i) == i) {
                    i++; // offset by length of keyword - 1
                    tokens.push_back(Token(err, "==", TokenType::OP_EQ));
                } else {
                    tokens.push_back(Token(err, "=", TokenType::ASSIGN));
                }
                continue;
            }
        }

        // invalid token
        throw TInvalidTokenException(err);
    }
}