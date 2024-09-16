#ifndef __ERR_INFO_HPP
#define __ERR_INFO_HPP

// for error handling
typedef unsigned long long line_t; // for line/col numbering

// store where a token is from in its original file
class ErrInfo {
    public:
        ErrInfo(line_t line, line_t col) : line(line), col(col) {};
        line_t line, col;
};

#endif