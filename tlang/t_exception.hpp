#ifndef __T_EXCEPTION_HPP
#define __T_EXCEPTION_HPP

#include <stdexcept>

#include "toolbox.hpp"

// shorthand for making exceptions
#define MAKE_EXCEPTION(name) class T##name##Exception : public TException { \
    public: \
        T##name##Exception(ErrInfo err) : TException(err) { msg = #name "Exception " + getTrace(); } \
};

class TException {
    public:
        TException(ErrInfo err) : err(err), msg("Base TException.") {throw 1;};

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
MAKE_EXCEPTION(InvalidToken)
MAKE_EXCEPTION(UnclosedQuote)
MAKE_EXCEPTION(InvalidEscape)
MAKE_EXCEPTION(UnclosedComment)
MAKE_EXCEPTION(UnknownIdentifier)
MAKE_EXCEPTION(IdentifierInUse)

#endif