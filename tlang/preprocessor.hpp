#ifndef __PREPROCESSOR_HPP
#define __PREPROCESSOR_HPP

#include <filesystem>
#include <map>
#include <stack>
#include <string>
#include <vector>

#include "../util/globals.hpp"

#include "util/token.hpp"
#include "util/t_exception.hpp"

// preprocesses a given document
typedef std::map<std::string, std::string> macrodef_map;
typedef std::stack<std::filesystem::path> cwd_stack;

bool preprocessLine(std::string, macrodef_map&, std::vector<Token>&, cwd_stack&, const ErrInfo);

void replaceMacrodefs(std::string&, macrodef_map&, size_t=0);

#endif