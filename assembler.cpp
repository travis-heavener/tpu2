#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "assembler.hpp"
#include "memory.hpp"
#include "tpu.hpp"

#define MAIN_LABEL_NAME "main"

// abstractions from processLine for readability
void parseMOV(const std::vector<std::string>&, Memory&, u16&);
void parseADDSUBLogic(const std::vector<std::string>&, Memory&, u16&, OPCode);
void parseMULDIV(const std::vector<std::string>&, Memory&, u16&, bool);
void parseNOT(const std::vector<std::string>&, Memory&, u16&);
void parsePUSH(const std::vector<std::string>&, Memory&, u16&);
void parsePOP(const std::vector<std::string>&, Memory&, u16&);
void parseBitShifts(const std::vector<std::string>&, Memory&, u16&, bool);

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

void trimString(std::string& str) {
    ltrimString(str);
    rtrimString(str);
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

// shorthand to check args and throw error if invalid size
void checkArgs(const std::vector<std::string>& args, u8 size) {
    if (args.size() != size)
        throw std::invalid_argument("Invalid number of arguments.");
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
        trimString(buf); // trim buffered split
        if (buf.size() == 0) continue; // skip empty buffers

        // collapse characters into raw bytes
        if (buf[0] == '\'') {
            if (*buf.rbegin() != '\'') // handle unclosed characters
                throw std::invalid_argument("Unclosed character.");
            
            // verify char-string is one character long
            if ( !(buf.length() == 3 && buf[1] != '\\') && !(buf.length() == 4 && buf[1] == '\\') )
                throw std::invalid_argument("Invalid character: " + buf);
            
            // parse char-string to primitive character
            args.push_back(std::to_string( (u16)parseCharacter(buf.substr(1, buf.length()-2)) ));
            continue;
        }

        // handle addresses
        bool isAddr = buf[0] == '@';
        if (isAddr) buf.erase(buf.begin(), buf.begin()+1);

        // convert hex & binary to decimal
        const std::string firstTwoChars = buf.substr(0, 2);
        if (buf.size() > 2 && (firstTwoChars == "0x" || firstTwoChars == "0d" || firstTwoChars == "0b")) {
            u8 base = firstTwoChars == "0x" ? 16 : firstTwoChars == "0b" ? 2 : 10;
            unsigned long long num = std::stoul(buf.substr(2), nullptr, base);
            
            // prevent overflow
            if (num > 0xFFFF) throw std::invalid_argument("Numeric literal exceeds 0xFFFF.");
            buf = std::to_string(num);
        }

        // mark as an address
        if (isAddr) buf.insert(buf.begin(), '@');

        args.push_back(buf); // append argument
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
    u16 instIndex = FREE_LOWER_ADDR;
    std::string line;
    std::map<std::string, u16> labelMap; // label name, start address
    
    // for labels that come after instIndex
    std::vector<std::pair<std::string, u16>> labelsToReplace; // [ label name, replacement start address ]
    
    while (std::getline(inHandle, line)) {
        processLine(line, memory, instIndex, labelMap, labelsToReplace); // process the line
    }

    // jump to mainEntryAddr
    if (labelMap.count(MAIN_LABEL_NAME) == 0)
        throw std::invalid_argument("No main label found in file.");

    // fill in any labels with addresses
    for (auto labelPair : labelsToReplace) {
        if (labelMap.count(labelPair.first) == 0)
            throw std::invalid_argument("Could not find label: " + labelPair.first);
        
        u16 destAddr = labelMap[labelPair.first];
        u16 addr = labelPair.second;
        memory[ addr ] = destAddr & 0x00FF;
        memory[addr+1] = (destAddr & 0xFF00) >> 8;
    }

    u16 mainEntryAddr = labelMap.at(MAIN_LABEL_NAME);
    memory[INSTRUCTION_PTR_START] = OPCode::JMP;
    memory[INSTRUCTION_PTR_START+1] = 0; // MOD byte
    memory[INSTRUCTION_PTR_START+2] = mainEntryAddr & 0x00FF; // lower-half of addr
    memory[INSTRUCTION_PTR_START+3] = (mainEntryAddr & 0xFF00) >> 8; // upper-half of addr

    // close file
    inHandle.close();
}

// process an individual line and load it into memory
void processLine(std::string& line, Memory& memory, u16& instIndex, std::map<std::string, u16>& labelMap,
                 std::vector<std::pair<std::string, u16>>& labelsToReplace) {
    stripComments(line); // remove comments
    trimString(line); // ltrim & rtrim string

    if (line.length() == 0) return; // ignore empty strings

    // grab keyword
    size_t spaceIndex = line.find(' ');
    std::string kwd = line.substr(0, spaceIndex);

    // grab args
    std::vector<std::string> args;
    if (spaceIndex != std::string::npos) loadInstructionArgs(line.substr(spaceIndex), args);

    // handle each instruction
    if (kwd == "nop") {
        checkArgs(args, 0); // check for extra args
        memory[instIndex++] = OPCode::NOP; // add instruction
    } else if (kwd == "hlt") {
        checkArgs(args, 0); // check for extra args
        memory[instIndex++] = OPCode::HLT; // add instruction
    } else if (kwd == "syscall") {
        checkArgs(args, 0); // check for extra args
        memory[instIndex++] = OPCode::SYSCALL; // add instruction
    } else if (kwd == "call") {
        checkArgs(args, 1); // check for extra args
        memory[instIndex++] = OPCode::CALL;

        // get address of label from map
        if (labelMap.count(args[0]) == 0) {
            u16 startAddr = instIndex;
            instIndex += 2; // make space for address
            labelsToReplace.push_back({args[0], startAddr});
        } else {
            u16 destAddr = labelMap[args[0]];
            memory[instIndex++] = destAddr & 0x00FF; // lower-half
            memory[instIndex++] = (destAddr & 0xFF00) >> 8; // upper-half
        }
    } else if (kwd == "jmp" || kwd == "jz" || kwd == "jnz" || kwd == "jc" || kwd == "jnc") {
        checkArgs(args, 1); // check for extra args
        memory[instIndex++] = OPCode::JMP;
        memory[instIndex++] = kwd == "jmp" ? 0 : kwd == "jz" ? 1 : kwd == "jnz" ? 2 : kwd == "jc" ? 3 : 4; // MOD byte

        // get address of label from map
        if (labelMap.count(args[0]) == 0) {
            u16 startAddr = instIndex;
            instIndex += 2; // make space for address
            labelsToReplace.push_back({args[0], startAddr});
        } else {
            u16 destAddr = labelMap[args[0]];
            memory[instIndex++] = destAddr & 0x00FF; // lower-half
            memory[instIndex++] = (destAddr & 0xFF00) >> 8; // upper-half
        }
    } else if (kwd == "mov") {
        checkArgs(args, 2); // check for extra args
        parseMOV(args, memory, instIndex);
    } else if (kwd == "push") {
        checkArgs(args, 1); // check for extra args
        parsePUSH(args, memory, instIndex);
    } else if (kwd == "pop") {
        if (args.size() > 1) throw std::invalid_argument("Invalid number of arguments.");
        parsePOP(args, memory, instIndex);
    } else if (kwd == "ret") {
        checkArgs(args, 0); // check for extra args
        memory[instIndex++] = OPCode::RET;
    } else if (kwd == "add" || kwd == "sub" || kwd == "and" || kwd == "or" || kwd == "xor") {
        checkArgs(args, 2); // check for extra args
        OPCode code = kwd == "add" ? OPCode::ADD : kwd == "sub" ? OPCode::SUB :
                      kwd == "and" ? OPCode::AND : kwd == "or" ? OPCode::OR : OPCode::XOR;
        parseADDSUBLogic(args, memory, instIndex, code);
    } else if (kwd == "mul" || kwd == "div") {
        checkArgs(args, 1); // check for extra args
        parseMULDIV(args, memory, instIndex, kwd == "mul");
    } else if (kwd == "not") {
        checkArgs(args, 1); // check for extra args
        parseNOT(args, memory, instIndex);
    } else if (*kwd.rbegin() == ':') { // label name
        checkArgs(args, 0); // verify rest of line is empty
        std::string labelName = kwd.substr(0, kwd.size()-1);

        // verify label name is valid
        if (kwd.size() == 1) throw std::invalid_argument("Invalid label name: " + kwd);

        labelMap[labelName] = instIndex; // store entry point
    } else if (kwd == "shl" || kwd == "shr") {
        checkArgs(args, 2); // check for extra args
        parseBitShifts(args, memory, instIndex, kwd == "shl");
    } else {
        // invalid instruction
        throw std::invalid_argument("Invalid instruction: " + kwd);
    }
}

// abstraction to parse a MOV instruction
void parseMOV(const std::vector<std::string>& args, Memory& memory, u16& instIndex) {
    // determine MOD byte
    u8 MOD = 0;
    std::vector<u8> bytesToWrite;
    switch (args[0][0]) {
        case '@': { // 0 & 1
            u16 addr = std::stoul(args[0].substr(1)); // dest
            bytesToWrite.push_back(addr & 0x00FF); // lower half
            bytesToWrite.push_back((addr & 0xFF00) >> 8); // upper half

            Register regB;
            try { // try as register (1)
                regB = getRegisterFromString(args[1]);
            } catch(std::invalid_argument&) { // try as imm8 (0)
                u32 arg = std::stoul(args[1]);
                if (arg > 0xFF) throw std::invalid_argument("Expected 8-bit literal.");
                MOD = 0;
                bytesToWrite.push_back((u8)arg); // imm8
                break;
            }

            // base case, try register
            // ensure register is 8-bit
            if (!isRegister8Bit(regB)) throw std::invalid_argument("Expected 8-bit register.");
            MOD = 1; // MOD byte
            bytesToWrite.push_back(regB); // src register
            break;
        }
        default: { // 2-6
            // get register
            Register regA = getRegisterFromString(args[0]);
            bytesToWrite.push_back(regA); // dest
            bool isRegA8 = isRegister8Bit(regA);

            // try second operand as addr (4)
            if (args[1][0] == '@') {
                if (!isRegA8) throw std::invalid_argument("Expected 8-bit register.");
                u16 addr = std::stoul(args[1].substr(1));
                MOD = 4;
                bytesToWrite.push_back(addr & 0x00FF); // lower half
                bytesToWrite.push_back((addr & 0xFF00) >> 8); // upper half
            } else {
                // try second operand as register (5-6)
                Register regB;
                try {
                    regB = getRegisterFromString(args[1]);
                } catch (std::invalid_argument&) {
                    // try as imm8 or imm16 (2-3)
                    u32 arg = std::stoul(args[1]);
                    bytesToWrite.push_back(arg & 0x00FF); // lower half
                    if (isRegA8) { // try as imm8 (2)
                        if (arg > 0xFF) throw std::invalid_argument("Expected 8-bit literal.");
                        MOD = 2;
                    } else { // try as imm16 (3)
                        if (arg > 0xFFFF) throw std::invalid_argument("Expected 16-bit literal.");
                        MOD = 3;
                        bytesToWrite.push_back((arg & 0xFF00) >> 8); // upper half
                    }
                    break;
                }

                // base case, it's a register->register move
                bool isRegB8 = isRegister8Bit(regB); // prevent register mismatch
                if (isRegA8 != isRegB8) throw std::invalid_argument("8-bit and 16-bit register mismatch.");
                
                MOD = isRegA8 ? 5 : 6; // try as 8-bit (5) or 16-bit (6)
                bytesToWrite.push_back(regB); // src
            }
            break;
        }
    }

    // write bytes
    memory[instIndex++] = OPCode::MOV;
    memory[instIndex++] = MOD;
    for (u8 b : bytesToWrite) memory[instIndex++] = b;
}

// ADD, SUB, AND, OR, and XOR all use the same argument & MOD byte patterns
void parseADDSUBLogic(const std::vector<std::string>& args, Memory& memory, u16& instIndex, OPCode instruction) {
    std::vector<u8> bytesToWrite;
    
    // determine target register
    Register reg = getRegisterFromString(args[0]);
    bool isRegA8 = isRegister8Bit(reg);
    bytesToWrite.push_back(reg);

    // determine MOD byte
    u8 MOD = 0;
    try { // try second operand as register
        Register regB = getRegisterFromString(args[1]);
        bool isRegB8 = isRegister8Bit(regB); // prevent register mismatch
        if (isRegA8 != isRegB8) throw std::invalid_argument("8-bit and 16-bit register mismatch.");
        MOD = isRegA8 ? 2 : 3; // try as 8-bit (2) or 16-bit (3)
        bytesToWrite.push_back(regB); // src
    } catch (std::invalid_argument&) { // try as imm8/16
        // try as imm8 or imm16 (0-1)
        u32 arg = std::stoul(args[1]);
        bytesToWrite.push_back(arg & 0x00FF); // lower half
        
        if (isRegA8) { // try as imm8 (0)
            if (arg > 0xFF) throw std::invalid_argument("Expected 8-bit literal.");
            MOD = 0;
        } else { // try as imm16 (1)
            if (arg > 0xFFFF) throw std::invalid_argument("Expected 16-bit literal.");
            MOD = 1;
            bytesToWrite.push_back((arg & 0xFF00) >> 8); // upper half
        }
    }

    // write bytes
    memory[instIndex++] = instruction;
    memory[instIndex++] = MOD;
    for (u8 b : bytesToWrite) memory[instIndex++] = b;
}

// MUL & DIV both use the same argument & MOD byte patterns
void parseMULDIV(const std::vector<std::string>& args, Memory& memory, u16& instIndex, bool isMul) {
    std::vector<u8> bytesToWrite;

    // determine MOD byte
    u8 MOD = 0;
    try { // try second operand as register
        Register reg = getRegisterFromString(args[0]);
        MOD = isRegister8Bit(reg) ? 2 : 3; // try as 8-bit (2) or 16-bit (3)
        bytesToWrite.push_back(reg); // src
    } catch (std::invalid_argument&) { // try as imm8/16
        // try as imm8 or imm16 (0-1)
        u32 arg = std::stoul(args[0]);
        bytesToWrite.push_back(arg & 0x00FF); // lower half
        
        if (arg > 0xFFFF) {
            throw std::invalid_argument("Expected 16-bit literal.");
        } else if (arg > 0xFF) { // try as imm16 (0)
            MOD = 1;
            bytesToWrite.push_back((arg & 0xFF00) >> 8); // upper half
        } else { // try as imm8 (0)
            MOD = 0;
        }
    }

    // write bytes
    memory[instIndex++] = isMul ? OPCode::MUL : OPCode::DIV;
    memory[instIndex++] = MOD;
    for (u8 b : bytesToWrite) memory[instIndex++] = b;
}

void parseNOT(const std::vector<std::string>& args, Memory& memory, u16& instIndex) {
    Register reg = getRegisterFromString(args[0]);
    memory[instIndex++] = OPCode::NOT;
    memory[instIndex++] = isRegister8Bit(reg) ? 0 : 1; // MOD byte
    memory[instIndex++] = reg;
}

void parsePUSH(const std::vector<std::string>& args, Memory& memory, u16& instIndex) {
    memory[instIndex++] = OPCode::PUSH;

    try { // try as register
        Register reg = getRegisterFromString(args[0]);
        if (!isRegister8Bit(reg))
            throw std::invalid_argument("Expected 8-bit register.");
        memory[instIndex++] = 0; // MOD byte
        memory[instIndex++] = reg;
    } catch (std::invalid_argument&) { // try as imm8
        u16 arg = std::stoul(args[0]);
        if (arg > 0xFF) throw std::invalid_argument("Expected 8-bit literal.");
        memory[instIndex++] = 1; // MOD byte
        memory[instIndex++] = arg;
    }
}

void parsePOP(const std::vector<std::string>& args, Memory& memory, u16& instIndex) {
    memory[instIndex++] = OPCode::POP;
    memory[instIndex++] = 1 - args.size(); // MOD byte

    if (args.size() == 1) { // verify argument is reg8
        Register reg = getRegisterFromString(args[0]);
        if (!isRegister8Bit(reg))
            throw std::invalid_argument("Expected 8-bit register.");
        memory[instIndex++] = reg;
    }
}

void parseBitShifts(const std::vector<std::string>& args, Memory& memory, u16& instIndex, bool isLeftShift) {
    memory[instIndex++] = isLeftShift ? OPCode::SHL : OPCode::SHR;

    // get register
    Register reg = getRegisterFromString(args[0]);
    memory[instIndex++] = !isRegister8Bit(reg); // MOD byte
    memory[instIndex++] = reg;

    // get imm8
    u16 arg = std::stoul(args[1]);
    if (arg > 0xFF) throw std::invalid_argument("Expected 8-bit literal.");
    memory[instIndex++] = (u8)arg;
}