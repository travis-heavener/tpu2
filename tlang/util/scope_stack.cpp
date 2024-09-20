#include <string>

#include "scope_stack.hpp"
#include "type.hpp"
#include "err_info.hpp"
#include "t_exception.hpp"

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

    // force the type to have an lvalue status
    parserScope[name].setIsLValue(true);
}