#ifndef __T_ASSEMBLER_HPP
#define __T_ASSEMBLER_HPP

#include <fstream>

#include "ast/ast.hpp"

// generate TPU assembly code from the AST
void generateAssembly(AST&, std::ofstream&);

#endif