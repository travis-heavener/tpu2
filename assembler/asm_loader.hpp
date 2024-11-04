#ifndef __ASM_LOADER_HPP
#define __ASM_LOADER_HPP

#include <map>
#include <regex>
#include <string>
#include <vector>

#include "../util/globals.hpp"
#include "../memory.hpp"

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
typedef std::vector<std::pair<std::string, u16>> label_replace_vec_t;

// used to resolve an argument into its corresponding numeric value
#define ARG_REG8            0
#define ARG_REG16           1
#define ARG_IMM8            2
#define ARG_IMM16           3
#define ARG_LABEL           4
#define ARG_ADDR_DIRECT     5
#define ARG_ADDR_OFFSET     6

// regexs for argument resolving
#define __REGEX_REG16 "(([ABCD]X)|([SBCI]P)|([SD]I))"
#define __REGEX_IMMED "(-?((0[Bb][01]+)|(0[Xx][abcdefABCDEF\\d]+)|(\\d+)))"
const std::regex RE_ARG_REG8("([ABCD][LH])");
const std::regex RE_ARG_REG16("^" __REGEX_REG16 "$");
const std::regex RE_ARG_IMMED("^" __REGEX_IMMED "$");
const std::regex RE_ARG_LABEL("^([^-\\d]\\w+)$");
const std::regex RE_ARG_ADDR_DIRECT("^(@" __REGEX_IMMED ")$");
const std::regex RE_ARG_ADDR_OFFSET("^(" __REGEX_IMMED "\\(" __REGEX_REG16 "\\))$");
u8 resolveArgument(const std::string&, std::vector<u8>&, const bool=false, const bool=false);

// responsible for taking a .tpu file and loading it into memory for main
u16 loadFileToMemory(const std::string&, Memory&);

// process an individual line from .text section and load it into memory
void processLineToText(std::string&, Memory&, u16&, label_map_t&, std::vector<std::pair<std::string, u16>>&);

// process an individual line from .data section and load it into memory
void processLineToData(std::string&, Memory&, u16&, label_map_t&);

#endif