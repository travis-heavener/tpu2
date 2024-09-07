#ifndef __TOOLBOX_HPP
#define __TOOLBOX_HPP

#include <string>

// helper to check if file exists
// modified from https://stackoverflow.com/a/6296808
bool doesFileExist(const std::string& file);

// for error handling
typedef unsigned long long line_t; // for line/col numbering

// store where a token is from in its original file
class ErrInfo {
    public:
        ErrInfo(line_t line, line_t col) : line(line), col(col) {};
        line_t line;
        line_t col;
};

// for lexer
enum TokenType {
    
};

class Token {
    public:
        Token(ErrInfo err, std::string raw, TokenType type): err(err), raw(raw), type(type) {};
        ErrInfo err;
        const std::string raw;
        TokenType type;
};

#endif