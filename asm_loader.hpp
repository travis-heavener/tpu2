#ifndef __ASM_LOADER_HPP
#define __ASM_LOADER_HPP

#include <map>
#include <string>
#include <vector>

#include "util/globals.hpp"
#include "memory.hpp"

#define SECTION_NONE 0
#define SECTION_TEXT 1
#define SECTION_DATA 2

#define DATA_TYPE_DEFAULT ""

class Label {
    public:
        Label() : type(DATA_TYPE_DEFAULT) {};
        Label(u16 addr) : type(DATA_TYPE_DEFAULT), value(addr) {};
        Label(const std::string& type, u16 addr) : type(type), value(addr) {};
        const std::string type;
        u16 value;
};

typedef std::map<std::string, Label> label_map_t;

// responsible for taking a .tpu file and loading it into memory for main
void loadFileToMemory(const std::string&, Memory&);

// process an individual line from .text section and load it into memory
void processLineToText(std::string&, Memory&, u16&, label_map_t&, std::vector<std::pair<std::string, u16>>&);

// process an individual line from .data section and load it into memory
void processLineToData(std::string&, Memory&, u16&, label_map_t&);

#endif