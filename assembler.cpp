#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "assembler.hpp"
#include "memory.hpp"
#include "tpu.hpp"

// helper for trimming strings in place
void ltrimString(std::string& str) {
    while (str.length() > 0 && std::isspace(str[0])) {
        str.erase(str.begin(), str.begin()+1);
    }
}

void rtrimString(std::string& str) {
    while (str.length() > 0 && std::isspace(*str.rbegin())) {
        str.erase(str.end()-1, str.end());
    }
}

// remove comments from a string
void stripComments(std::string& line) {
    size_t commentStart = std::string::npos;
    bool inChar = false;
    for (size_t i = 0; i < line.length(); i++) {
        if (!inChar && line[i] == '\'') {
            inChar = true;
        } else if (inChar && line[i] == '\\') {
            i++;
        } else if (inChar && line[i] == '\'') {
            inChar = false;
        } else if (!inChar && line[i] == ';') {
            commentStart = i;
            break;
        }
    }

    // unclosed char
    if (inChar)
        throw std::invalid_argument("Unclosed character.");

    // remove comment if found
    if (commentStart != std::string::npos)
        line.erase(line.begin() + commentStart, line.end());
}

// parse a character string to a primitive character
char parseCharacter(const std::string& charString) {
    if (charString[0] == '\\') {
        switch (charString[1]) { // escape character
            case 'a': return '\a';
            case 'b': return '\b';
            case 't': return '\t';
            case 'n': return '\n';
            case 'v': return '\v';
            case 'f': return '\f';
            case 'r': return '\r';
            case 'e': return '\e';
            default: return charString[1];
        }
    } else { // unescaped character
        return charString[0];
    }
}

// extract instruction arguments from line
void loadInstructionArgs(const std::string& line, std::vector<std::string>& args) {
    // ltrim the line (already rtrimmed)
    std::string lineCopy(line), buf;
    ltrimString(lineCopy);

    // create stringstream from line copy
    std::stringstream sstream(lineCopy);

    // split on commas
    while (std::getline(sstream, buf, ',')) {
        // trim buffered split
        ltrimString(buf);
        rtrimString(buf);

        // skip empty buffers
        if (buf.size() == 0) continue;

        // collapse characters into raw bytes
        if (buf[0] == '\'') {
            if (*buf.rbegin() != '\'') // handle unclosed characters
                throw std::invalid_argument("Unclosed character.");
            
            // verify char-string is one character long
            if ( !(buf.length() == 3 && buf[1] != '\\') && !(buf.length() == 4 && buf[1] == '\\') )
                throw std::invalid_argument("Invalid character: " + buf);
            
            // parse char-string to primitive character
            args.push_back(
                std::to_string( (u16)parseCharacter(buf.substr(1, buf.length()-2)) )
            );
            continue;
        }

        // handle addresses
        bool isAddr = buf[0] == '@';
        if (isAddr) buf.erase(buf.begin(), buf.begin()+1);

        // convert hex & binary to decimal
        if (buf.size() > 2 && buf.substr(0, 2) == "0x") {
            unsigned long long num = std::stoul(buf.substr(2), nullptr, 16);
            if (num > 0xFFFF) throw std::invalid_argument("Numeric literal exceeds 0xFFFF.");
            buf = std::to_string(num);
        } else if (buf.size() > 2 && buf.substr(0, 2) == "0b") {
            unsigned long long num = std::stoul(buf.substr(2), nullptr, 2);
            if (num > 0xFFFF) throw std::invalid_argument("Numeric literal exceeds 0xFFFF.");
            buf = std::to_string(num);
        } else if (buf.size() > 2 && buf.substr(0, 2) == "0d") {
            unsigned long long num = std::stoul(buf.substr(2));
            if (num > 0xFFFF) throw std::invalid_argument("Numeric literal exceeds 0xFFFF.");
            buf = std::to_string(num);
        }

        // denote an address
        if (isAddr) buf.insert(buf.begin(), '@');

        // append argument
        args.push_back(buf);
    }
}

// abstraction to parse a MOV instruction
void parseMOV(const std::vector<std::string>& args, Memory& memory, u16& instIndex) {
    if (args.size() != 2) // check for extra args
        throw std::invalid_argument("Invalid number of arguments.");
    memory[instIndex++] = OPCode::MOV;
    
    // determine MOD byte
    switch (args[0][0]) {
        case '@': { // 0 & 1
            u16 addr = std::stoul(args[0].substr(1)); // dest

            try { // try as register (1)
                Register reg = getRegisterFromString(args[1]);
                memory[instIndex++] = 1; // MOD byte
                memory[instIndex++] = addr & 0x00FF; // lower half
                memory[instIndex++] = (addr & 0xFF00) >> 8; // upper half
                memory[instIndex++] = reg;
            } catch(std::invalid_argument&) {
                // try as imm8 (0)
                u32 arg = std::stoul(args[1]);
                if (arg > 0xFF) throw std::invalid_argument("Expected 8-bit literal.");
                memory[instIndex++] = 0; // MOD byte
                memory[instIndex++] = addr & 0x00FF; // lower half
                memory[instIndex++] = (addr & 0xFF00) >> 8; // upper half
                memory[instIndex++] = (u8)arg;
            }
            break;
        }
        default: { // 2-6
            // get register
            Register regA = getRegisterFromString(args[0]);
            bool isRegA8 = isRegister8Bit(regA);

            // try second operand as addr (4)
            if (args[1][0] == '@') {
                if (!isRegA8) throw std::invalid_argument("Expected 8-bit register.");
                
                u16 addr = std::stoul(args[1].substr(1));
                memory[instIndex++] = 4; // MOD byte
                memory[instIndex++] = regA; // dest
                memory[instIndex++] = addr & 0x00FF; // lower half
                memory[instIndex++] = (addr & 0xFF00) >> 8; // upper half
            } else {
                // try second operand as register (5-6)
                Register regB;
                try {
                    regB = getRegisterFromString(args[1]);
                } catch (std::invalid_argument&) {
                    // try as imm8 or imm16 (2-3)
                    u32 arg = std::stoul(args[1]);
                    if (isRegA8) { // try as imm8 (2)
                        if (arg > 0xFF) throw std::invalid_argument("Expected 8-bit literal.");
                        memory[instIndex++] = 2; // MOD byte
                        memory[instIndex++] = regA; // dest
                        memory[instIndex++] = (u8)arg;
                    } else { // try as imm16 (3)
                        if (arg > 0xFFFF) throw std::invalid_argument("Expected 16-bit literal.");
                        memory[instIndex++] = 3; // MOD byte
                        memory[instIndex++] = regA; // dest
                        memory[instIndex++] = arg & 0x00FF; // lower half
                        memory[instIndex++] = (arg & 0xFF00) >> 8; // upper half
                    }
                    break;
                }

                // treat as register (5-6)
                bool isRegB8 = isRegister8Bit(regB);
                if (isRegA8 && isRegB8) { // try as 8-bit (5)
                    memory[instIndex++] = 5; // MOD byte
                } else if (!isRegA8 && !isRegB8) { // try as 16-bit (6)
                    memory[instIndex++] = 6; // MOD byte
                } else {
                    throw std::invalid_argument("8-bit and 16-bit register mismatch.");
                }
                memory[instIndex++] = regA; // dest
                memory[instIndex++] = regB; // src
            }
            break;
        }
    }
}

// responsible for taking a .tpu file and loading it into memory for main
void loadFileToMemory(const std::string& path, Memory& memory) {
    // open file
    std::ifstream inHandle(path);

    if (!inHandle.is_open()) {
        std::cerr << "Failed to open file: " + path << std::endl;
        exit(1);
    }

    // read each line
    u16 instIndex = INSTRUCTION_PTR_START;
    std::string line;
    while (std::getline(inHandle, line)) {
        // process the line
        processLine(line, memory, instIndex);
    }

    // close file
    inHandle.close();
}

// process an individual line and load it into memory
void processLine(std::string& line, Memory& memory, u16& instIndex) {
    // remove comments
    stripComments(line);

    // ltrim & rtrim string
    ltrimString(line);
    rtrimString(line);

    // ignore empty strings
    if (line.length() == 0) return;

    // grab keyword
    size_t spaceIndex = line.find(' ');
    std::string kwd = line.substr(0, spaceIndex);

    // grab args
    std::vector<std::string> args;
    if (spaceIndex != std::string::npos)
        loadInstructionArgs(line.substr(spaceIndex), args);

    // handle each instruction
    if (kwd == "nop") {
        if (args.size() != 0) // check for extra args
            throw std::invalid_argument("Invalid number of arguments.");
        memory[instIndex++] = OPCode::NOP; // add instruction
    } else if (kwd == "hlt") {
        if (args.size() != 0) // check for extra args
            throw std::invalid_argument("Invalid number of arguments.");
        memory[instIndex++] = OPCode::HLT; // add instruction
    } else if (kwd == "syscall") {
        if (args.size() != 0) // check for extra args
            throw std::invalid_argument("Invalid number of arguments.");
        memory[instIndex++] = OPCode::SYSCALL; // add instruction
    } else if (kwd == "jmp" || kwd == "jz" || kwd == "jnz") {
        if (args.size() != 1) // check for extra args
            throw std::invalid_argument("Invalid number of arguments.");
        memory[instIndex++] = OPCode::JMP;
        memory[instIndex++] = kwd == "jmp" ? 0 : kwd == "jz" ? 1 : 2; // MOD byte

        // verify address argument
        if (args[0][0] != '@') throw std::invalid_argument("Expected memory address.");

        u16 addr = std::stoul(args[0]);
        memory[instIndex++] = addr & 0x00FF; // lower half
        memory[instIndex++] = (addr & 0xFF00) >> 8; // upper half
    } else if (kwd == "mov") {
        parseMOV(args, memory, instIndex);
    } else {
        // invalid instruction
        throw std::invalid_argument("Invalid instruction: " + kwd);
    }
}
