#ifndef __ASSEMBLER_HPP
#define __ASSEMBLER_HPP

#include <map>
#include <string>
#include <vector>

#include "memory.hpp"

// responsible for taking a .tpu file and loading it into memory for main
void loadFileToMemory(const std::string&, Memory&);

// process an individual line and load it into memory
void processLine(std::string&, Memory&, u16&, std::map<std::string, u16>&, std::vector<std::pair<std::string, u16>>&);

#endif