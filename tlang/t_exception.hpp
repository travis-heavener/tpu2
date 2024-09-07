#ifndef __T_EXCEPTION_HPP
#define __T_EXCEPTION_HPP

#include <stdexcept>

#include "toolbox.hpp"

// shorthand for making exceptions
#define MAKE_EXCEPTION(name) class T##name : public TException { \
    public: \
        T##name(ErrInfo err) : TException(err) { msg = #name "Exception " + getTrace(); } \
};

class TException {
    public:
        TException(ErrInfo err) : err(err), msg("Base TException.") {};

        // virtual toString method
        virtual const std::string& toString() const { return msg; };
        
        // to get line/col info
        const std::string getTrace() const { return "at line " + std::to_string(err.line) + ":" + std::to_string(err.col); };
    protected:
        ErrInfo err;
        std::string msg;
};

// macro define exceptions

MAKE_EXCEPTION(UnclosedGroup)
MAKE_EXCEPTION(ZeroDiv)

#endif