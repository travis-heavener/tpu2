#ifndef __T_EXCEPTION_HPP
#define __T_EXCEPTION_HPP

#include <stdexcept>
#include <string>

// for error handling
typedef unsigned long long line_t; // for line/col numbering

// store where a token is from in its original file
class ErrInfo {
    public:
        ErrInfo(line_t line, line_t col, const std::string& file) : line(line), col(col), file(file) {};
        line_t line, col;
        const std::string file;
};

// shorthand for making exceptions
#define MAKE_EXCEPTION(name) class T##name##Exception : public TException { \
    public: \
        T##name##Exception(ErrInfo err) : TException(err) { msg = #name "Exception " + getTrace(); } \
};

class TException {
    public:
        TException(ErrInfo err) : err(err), msg("Base TException.") {throw 0;};
        TException(ErrInfo err, const std::string& msg) : err(err), msg(msg) {};

        // virtual toString method
        virtual const std::string& toString() const { return msg; };
        
        // to get line/col info
        const std::string getTrace() const { return "\n  " + err.file + ":" + std::to_string(err.line) + ":" + std::to_string(err.col); };
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
MAKE_EXCEPTION(UnknownFunction)
MAKE_EXCEPTION(IdentifierInUse)
MAKE_EXCEPTION(TypeInfer)
MAKE_EXCEPTION(InvalidOperation)
MAKE_EXCEPTION(Syntax)
MAKE_EXCEPTION(VoidReturn)
MAKE_EXCEPTION(MissingReturn)
MAKE_EXCEPTION(IllegalArraySize)
MAKE_EXCEPTION(IllegalImplicitCast)
MAKE_EXCEPTION(ExpressionEval)
MAKE_EXCEPTION(IllegalMacroDefinition)
MAKE_EXCEPTION(InvalidMacroInclude)
MAKE_EXCEPTION(IllegalVoidUse)
MAKE_EXCEPTION(ConstQualifierMismatch)
MAKE_EXCEPTION(ConstAssignment)
MAKE_EXCEPTION(AmbiguousFunctionResolution)
MAKE_EXCEPTION(FunctionParameterMismatch)

// dev exceptions
class TDevException : public TException {
    public:
        TDevException(const std::string& msg) : TException(ErrInfo(0, 0, "<core>"), msg) {};
};

#endif