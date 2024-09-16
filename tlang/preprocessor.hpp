#ifndef __PREPROCESSOR_HPP
#define __PREPROCESSOR_HPP

#include <filesystem>
#include <map>
#include <stack>
#include <string>
#include <vector>

#include "util/token.hpp"

// preprocesses a given document
typedef std::map<std::string, std::string> macrodef_map;
typedef std::stack<std::filesystem::path> cwd_stack;

bool preprocessLine(std::string, macrodef_map&, std::vector<Token>&, cwd_stack&);

void replaceMacrodefs(std::string&, macrodef_map&);

#endif