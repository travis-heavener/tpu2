#ifndef __SCOPE_STACK_HPP
#define __SCOPE_STACK_HPP

#include <map>
#include <string>
#include <vector>

#include "type.hpp"
#include "err_info.hpp"

// for parser scope stacks
typedef std::map<std::string, Type> parser_scope_t;
typedef std::vector<parser_scope_t> scope_stack_t;

// lookup variable from scope stack
Type lookupParserVariable(scope_stack_t&, const std::string&, ErrInfo);

// declare a variable in the immediate scope
void declareParserVariable(scope_stack_t&, const std::string&, Type, ErrInfo);

#endif