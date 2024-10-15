#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "asm_loader.hpp"
#include "memory.hpp"
#include "tpu.hpp"

// abstractions from processLineToText for readability
void parseMOV(const std::vector<std::string>&, Memory&, u16&);
void parseMOVW(const std::vector<std::string>&, Memory&, u16&, std::vector<std::pair<std::string, u16>>&);
void parseADDSUBLogic(const std::vector<std::string>&, Memory&, u16&, OPCode, bool);
void parseMULDIV(const std::vector<std::string>&, Memory&, u16&, bool, bool);
void parseNOTBUF(const std::vector<std::string>&, Memory&, u16&, OPCode);
void parsePUSH(const std::vector<std::string>&, Memory&, u16&, bool, std::vector<std::pair<std::string, u16>>&);
void parsePOP(const std::vector<std::string>&, Memory&, u16&);
void parsePOPW(const std::vector<std::string>&, Memory&, u16&);
void parseBitShifts(const std::vector<std::string>&, Memory&, u16&, bool, bool);

// returns true if a string is valid
bool isStringValid(const std::string& str) {
    // check for quotes
    if (str[0] != '"' || str.back() != '"') return false;

    // check each character
    for (size_t i = 1; i < str.size()-1; ++i) {
        if (str[i] == '\\') {
            if (i+1 == str.size()-1) return false;
            ++i;
        } else if (str[i] == '"') {
            return false;
        }
    }

    // base case, is valid
    return true;
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
    u16 instIndex = TEXT_LOWER_ADDR;
    u16 dataIndex = DATA_LOWER_ADDR;
    std::string line;
    label_map_t labelMap; // label name, start address
    
    // for labels that come after instIndex
    std::vector<std::pair<std::string, u16>> labelsToReplace; // [ label name, replacement start address ]

    int currentSection = SECTION_NONE;
    try {
        while (std::getline(inHandle, line)) {
            // handle switching the section
            std::string lineBuf = line;
            ltrimString(lineBuf);
            if (lineBuf.find("section ") == 0) {
                // get the section
                if (lineBuf.find(".data") == 8) {
                    currentSection = SECTION_DATA;
                } else if (lineBuf.find(".text") == 8) {
                    currentSection = SECTION_TEXT;
                } else {
                    // invalid section
                    throw std::invalid_argument("Invalid section: " + lineBuf.substr(8));
                }
                continue; // skip to next line
            }

            // switch based on the section
            switch (currentSection) {
                case SECTION_TEXT:
                    processLineToText(line, memory, instIndex, labelMap, labelsToReplace); // process the line
                    break;
                case SECTION_DATA:
                    processLineToData(line, memory, dataIndex, labelMap); // process the line
                    break;
                case SECTION_NONE: default:
                    throw std::invalid_argument("Cannot write to this section (use `section .data` or `section .text`).");
                    break;
            }
        }

        // jump to mainEntryAddr
        if (labelMap.count(RESERVED_LABEL_MAIN) == 0)
            throw std::invalid_argument("No main label found in file.");

        // fill in any labels with addresses
        for (auto labelPair : labelsToReplace) {
            if (labelMap.count(labelPair.first) == 0)
                throw std::invalid_argument("Could not find label: " + labelPair.first);

            Label label = labelMap[labelPair.first];
            if (label.type == DATA_TYPE_DEFAULT || label.type == DATA_TYPE_STRZ || label.type == DATA_TYPE_STR) {
                // replace with address
                u16 destAddr = labelMap[labelPair.first].value;
                u16 addr = labelPair.second;
                memory[ addr ] = destAddr & 0x00FF;
                memory[addr+1] = (destAddr & 0xFF00) >> 8;
            } else {
                throw std::invalid_argument("Invalid label type: " + labelPair.first);
            }
        }

        u16 mainEntryAddr = labelMap.at(RESERVED_LABEL_MAIN).value;
        memory[INSTRUCTION_PTR_START] = OPCode::JMP;
        memory[INSTRUCTION_PTR_START+1] = 0; // MOD byte
        memory[INSTRUCTION_PTR_START+2] = mainEntryAddr & 0x00FF; // lower-half of addr
        memory[INSTRUCTION_PTR_START+3] = (mainEntryAddr & 0xFF00) >> 8; // upper-half of addr

        // close file
        inHandle.close();
    } catch (std::invalid_argument& e) {
        // close inHandle
        inHandle.close();
        throw e;
    }
}

// process an individual line from .data section and load it into memory
void processLineToData(std::string& line, Memory& memory, u16& dataIndex, label_map_t& labelMap) {
    // normalize formatting
    trimString(line);

    // skip blank lines
    if (line.size() == 0) return;

    // check for the label name
    size_t startIndex = 0, spaceIndex = line.find(' ');
    if (spaceIndex == std::string::npos) throw std::invalid_argument("Invalid data declaration.");
    std::string labelName = line.substr(startIndex, spaceIndex);

    // check for the data type
    startIndex = spaceIndex+1;
    spaceIndex = line.find(' ', startIndex);
    if (spaceIndex == std::string::npos) throw std::invalid_argument("Invalid data declaration.");
    std::string dataType = line.substr(startIndex, spaceIndex-startIndex);

    // check for value
    std::string rawValue = line.substr(spaceIndex+1);
    if (rawValue.size() == 0) throw std::invalid_argument("Invalid data declaration.");

    // swtich on the data type provided
    trimString(labelName);
    trimString(dataType);
    trimString(rawValue);
    if (dataType == DATA_TYPE_STR || dataType == DATA_TYPE_STRZ) {
        // parse as string
        bool isNullTerminated = dataType == DATA_TYPE_STRZ;

        // verify string is valid
        if (!isStringValid(rawValue))
            throw std::invalid_argument("Invalid string in data declaration.");

        // extract raw value
        rawValue = rawValue.substr(1, rawValue.size()-2);
        escapeString(rawValue);

        // insert each character onto the document
        u16 startIndex = dataIndex;
        for (const char c : rawValue) {
            memory[dataIndex++] = (u8)c;
        }

        // add null terminator if needed
        if (isNullTerminated) memory[dataIndex++] = '\0';

        // insert into label map
        labelMap.insert({labelName, Label(dataType, startIndex)});
    } else {
        throw std::invalid_argument("Invalid data type: " + dataType);
    }
}

// process an individual line and load it into memory
void processLineToText(std::string& line, Memory& memory, u16& instIndex, label_map_t& labelMap,
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
            u16 destAddr = labelMap[args[0]].value;
            memory[instIndex++] = destAddr & 0x00FF; // lower-half
            memory[instIndex++] = (destAddr & 0xFF00) >> 8; // upper-half
        }
    } else if (kwd == "ret") {
        checkArgs(args, 0); // check for extra args
        memory[instIndex++] = OPCode::RET;
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
            u16 destAddr = labelMap[args[0]].value;
            memory[instIndex++] = destAddr & 0x00FF; // lower-half
            memory[instIndex++] = (destAddr & 0xFF00) >> 8; // upper-half
        }
    } else if (kwd == "mov") {
        checkArgs(args, 2); // check for extra args
        parseMOV(args, memory, instIndex);
    } else if (kwd == "movw") {
        checkArgs(args, 2); // check for extra args
        parseMOVW(args, memory, instIndex, labelsToReplace);
    } else if (kwd == "push" || kwd == "pushw") {
        checkArgs(args, 1); // check for extra args
        parsePUSH(args, memory, instIndex, kwd == "pushw", labelsToReplace);
    } else if (kwd == "pop") {
        if (args.size() > 1) throw std::invalid_argument("Invalid number of arguments.");
        parsePOP(args, memory, instIndex);
    } else if (kwd == "popw") {
        if (args.size() > 1) throw std::invalid_argument("Invalid number of arguments.");
        parsePOPW(args, memory, instIndex);
    } else if (kwd == "add" || kwd == "sub" || kwd == "sadd" || kwd == "ssub" || kwd == "and" || kwd == "or" || kwd == "xor" || kwd == "cmp" || kwd == "scmp") {
        checkArgs(args, 2); // check for extra args
        OPCode code = (kwd == "add" || kwd == "sadd") ? OPCode::ADD : (kwd == "ssub" || kwd == "sub") ? OPCode::SUB :
                      kwd == "and" ? OPCode::AND : kwd == "or" ? OPCode::OR : (kwd == "cmp" || kwd == "scmp") ? OPCode::CMP : OPCode::XOR;
        bool isSignedOp = kwd == "sadd" || kwd == "ssub" || kwd == "scmp";
        parseADDSUBLogic(args, memory, instIndex, code, isSignedOp);
    } else if (kwd == "mul" || kwd == "div" || kwd == "smul" || kwd == "sdiv") {
        checkArgs(args, 1); // check for extra args
        bool isSignedOp = kwd == "smul" || kwd == "sdiv";
        parseMULDIV(args, memory, instIndex, kwd == "mul" || kwd == "smul", isSignedOp);
    } else if (kwd == "not" || kwd == "buf") {
        checkArgs(args, 1); // check for extra args
        parseNOTBUF(args, memory, instIndex, kwd == "not" ? OPCode::NOT : OPCode::BUF);
    } else if (kwd.back() == ':') { // label name
        checkArgs(args, 0); // verify rest of line is empty
        std::string labelName = kwd.substr(0, kwd.size()-1);

        // verify label name is valid
        if (kwd.size() == 1) throw std::invalid_argument("Invalid label name: " + kwd);

        labelMap[labelName].value = instIndex; // store entry point
    } else if (kwd == "shl" || kwd == "shr" || kwd == "sshl" || kwd == "sshr") {
        checkArgs(args, 2); // check for extra args
        bool isSignedOp = kwd == "sshl" || kwd == "sshr";
        parseBitShifts(args, memory, instIndex, kwd == "shl" || kwd == "sshl", isSignedOp);
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

            // base case, try register (0)
            // ensure register is 8-bit
            if (!isRegister8Bit(regB)) throw std::invalid_argument("Expected 8-bit register.");
            MOD = 1; // MOD byte
            bytesToWrite.push_back(regB); // src register
            break;
        }
        default: { // 2-6
            // try first arg as register
            Register regA;
            try { // try for MOD 2, 3, 4, 6
                regA = getRegisterFromString(args[0]);
            } catch (std::invalid_argument&) { // must be offset<-reg (5)
                // check for open and close bracket
                // must have at least [RG-n]
                if (args[0].size() < 6) throw std::invalid_argument("Invalid offset for mov.");
                if (args[0][0] != '[') throw std::invalid_argument("Invalid offset for mov.");
                if (*args[0].rbegin() != ']') throw std::invalid_argument("Invalid offset for mov.");
                if (args[0][3] != '-' && args[0][3] != '+') throw std::invalid_argument("Invalid offset for mov.");

                // get register
                Register refReg = getRegisterFromString(args[0].substr(1, 2));
                if (refReg != Register::SP && refReg != Register::BP && refReg != Register::CP)
                    throw std::invalid_argument("Invalid register for mov.");

                // extract offset
                int offset = std::stoi(args[0].substr(3, args[0].size() - 4));
                if (offset < -0x8000 || offset > 0x7FFF) throw std::invalid_argument("Expected signed 16-bit literal.");
                MOD = 5;
                bytesToWrite.push_back(refReg);
                bytesToWrite.push_back((u8)(offset & 0x00FF));
                bytesToWrite.push_back((u8)((offset & 0xFF00) >> 8));

                // get src register
                Register regB = getRegisterFromString(args[1]);
                if (!isRegister8Bit(regB))
                    throw std::invalid_argument("Expected 8-bit register.");
                bytesToWrite.push_back(regB);
                break;
            }

            // base case, first arg IS register
            if (!isRegister8Bit(regA))
                throw std::invalid_argument("Expected 8-bit register.");
            bytesToWrite.push_back(regA); // dest

            // try second operand as addr (3)
            if (args[1][0] == '@') {
                u16 addr = std::stoul(args[1].substr(1));
                MOD = 3;
                bytesToWrite.push_back(addr & 0x00FF); // lower half
                bytesToWrite.push_back((addr & 0xFF00) >> 8); // upper half
            } if (args[1][0] == '[') { // try second operand as relative offset (6)
                // check for open and close bracket
                // must have at least [RG-n]
                if (args[1].size() < 6) throw std::invalid_argument("Invalid offset for mov.");
                if (args[1][0] != '[') throw std::invalid_argument("Invalid offset for mov.");
                if (*args[1].rbegin() != ']') throw std::invalid_argument("Invalid offset for mov.");
                if (args[1][3] != '-' && args[1][3] != '+') throw std::invalid_argument("Invalid offset for mov.");

                // get register
                Register refReg = getRegisterFromString(args[1].substr(1, 2));
                if (refReg != Register::SP && refReg != Register::BP && refReg != Register::CP)
                    throw std::invalid_argument("Invalid register for mov.");

                // extract offset
                int offset = std::stoi(args[1].substr(3, args[1].size() - 4));
                if (offset < -0x8000 || offset > 0x7FFF) throw std::invalid_argument("Expected signed 16-bit literal.");
                MOD = 6;
                bytesToWrite.push_back(refReg);
                bytesToWrite.push_back((u8)(offset & 0x00FF));
                bytesToWrite.push_back((u8)((offset & 0xFF00) >> 8));
            } else { // must be MOD 2 or 4
                Register regB;
                try { // try second operand as register (4)
                    regB = getRegisterFromString(args[1]);
                } catch (std::invalid_argument&) {
                    // try as imm8 (2)
                    MOD = 2;
                    u32 arg = std::stoul(args[1]);
                    if (arg > 0xFF) throw std::invalid_argument("Expected 8-bit literal.");
                    bytesToWrite.push_back(arg & 0x00FF); // lower half
                    break;
                }

                // base case MOD 4
                if (!isRegister8Bit(regB)) throw std::invalid_argument("Expected 8-bit register.");
                MOD = 4;
                bytesToWrite.push_back(regB);
            }
            break;
        }
    }

    // write bytes
    memory[instIndex++] = OPCode::MOV;
    memory[instIndex++] = MOD;
    for (u8 b : bytesToWrite) memory[instIndex++] = b;
}

void parseMOVW(const std::vector<std::string>& args, Memory& memory, u16& instIndex, std::vector<std::pair<std::string, u16>>& labelsToReplace) {
    memory[instIndex++] = OPCode::MOVW;

    // verify first operand is a 16-bit register
    Register regA = getRegisterFromString(args[0]);
    if (isRegister8Bit(regA))
        throw std::invalid_argument("Expected 16-bit register");
    
    // try second operand as register (1)
    Register regB;
    try {
        regB = getRegisterFromString(args[1]);
    } catch (std::invalid_argument&) {
        // try second operand as imm16 (0)
        memory[instIndex++] = 0; // MOD byte
        memory[instIndex++] = regA;
        try {
            u16 offset = std::stoul(args[1]);
            memory[instIndex++] = (u8)(offset & 0x00FF);
            memory[instIndex++] = (u8)((offset & 0xFF00) >> 8);
            return;
        } catch (std::invalid_argument&) {
            // make space for value
            u16 replaceAddr = instIndex;
            instIndex += 2;
            labelsToReplace.push_back({args[1], replaceAddr});
            return;
        }
    }

    // base case, is register (1)
    if (isRegister8Bit(regB))
        throw std::invalid_argument("Expected 16-bit register");

    memory[instIndex++] = 1; // MOD byte
    memory[instIndex++] = regA;
    memory[instIndex++] = regB;
}

// ADD, SUB, AND, OR, and XOR all use the same argument & MOD byte patterns
void parseADDSUBLogic(const std::vector<std::string>& args, Memory& memory, u16& instIndex, OPCode instruction, bool isSignedOp) {
    std::vector<u8> bytesToWrite;
    
    // determine target register
    Register reg = getRegisterFromString(args[0]);
    bool isRegA8 = isRegister8Bit(reg);
    bytesToWrite.push_back(reg);

    // determine MOD byte
    u8 MOD = isSignedOp ? 8 : 0;
    try { // try second operand as register
        Register regB = getRegisterFromString(args[1]);
        bool isRegB8 = isRegister8Bit(regB); // prevent register mismatch
        if (isRegA8 != isRegB8) throw std::runtime_error("8-bit and 16-bit register mismatch.");
        MOD |= isRegA8 ? 2 : 3; // try as 8-bit (2) or 16-bit (3)
        bytesToWrite.push_back(regB); // src
    } catch (std::invalid_argument&) { // try as imm8/16
        // try as imm8 or imm16 (0-1)
        u32 arg = std::stoul(args[1]);
        bytesToWrite.push_back(arg & 0x00FF); // lower half
        
        if (isRegA8) { // try as imm8 (0)
            if (arg > 0xFF) throw std::invalid_argument("Expected 8-bit literal.");
            MOD |= 0;
        } else { // try as imm16 (1)
            if (arg > 0xFFFF) throw std::invalid_argument("Expected 16-bit literal.");
            MOD |= 1;
            bytesToWrite.push_back((arg & 0xFF00) >> 8); // upper half
        }
    }

    // write bytes
    memory[instIndex++] = instruction;
    memory[instIndex++] = MOD;
    for (u8 b : bytesToWrite) memory[instIndex++] = b;
}

// MUL & DIV both use the same argument & MOD byte patterns
void parseMULDIV(const std::vector<std::string>& args, Memory& memory, u16& instIndex, bool isMul, bool isSignedOp) {
    std::vector<u8> bytesToWrite;

    // determine MOD byte
    u8 MOD = isSignedOp ? 8 : 0;
    try { // try second operand as register
        Register reg = getRegisterFromString(args[0]);
        MOD |= isRegister8Bit(reg) ? 2 : 3; // try as 8-bit (2) or 16-bit (3)
        bytesToWrite.push_back(reg); // src
    } catch (std::invalid_argument&) { // try as imm8/16
        // try as imm8 or imm16 (0-1)
        u32 arg = std::stoul(args[0]);
        bytesToWrite.push_back(arg & 0x00FF); // lower half
        
        if (arg > 0xFFFF) {
            throw std::invalid_argument("Expected 16-bit literal.");
        } else if (arg > 0xFF) { // try as imm16 (0)
            MOD |= 1;
            bytesToWrite.push_back((arg & 0xFF00) >> 8); // upper half
        } else { // try as imm8 (0)
            MOD |= 0;
        }
    }

    // write bytes
    memory[instIndex++] = isMul ? OPCode::MUL : OPCode::DIV;
    memory[instIndex++] = MOD;
    for (u8 b : bytesToWrite) memory[instIndex++] = b;
}

void parseNOTBUF(const std::vector<std::string>& args, Memory& memory, u16& instIndex, OPCode opCode) {
    memory[instIndex++] = opCode;

    if (opCode == OPCode::NOT) {
        Register reg = getRegisterFromString(args[0]);
        memory[instIndex++] = isRegister8Bit(reg) ? 0 : 1; // MOD byte
        memory[instIndex++] = reg;
    } else { // handle buf instruction
        // try for register
        try {
            Register reg = getRegisterFromString(args[0]);
            memory[instIndex++] = isRegister8Bit(reg) ? 0 : 1; // MOD byte
            memory[instIndex++] = reg;
        } catch (std::invalid_argument&) {
            // try as imm8/16
            u32 arg = std::stoul(args[0]);
            if (arg > 0xFFFF) throw std::invalid_argument("Expected 16-bit literal.");

            if (arg > 0xFF) { // imm16
                memory[instIndex++] = 3; // MOD byte
                memory[instIndex++] = arg & 0x00FF;
                memory[instIndex++] = (arg & 0xFF00) >> 8;
            } else { // imm8
                memory[instIndex++] = 2; // MOD byte
                memory[instIndex++] = arg & 0xFF;
            }
        }
    }
}

void parsePUSH(const std::vector<std::string>& args, Memory& memory, u16& instIndex, bool isPUSHW, std::vector<std::pair<std::string, u16>>& labelsToReplace) {
    memory[instIndex++] = OPCode::PUSH;

    Register reg;
    try { // try as register (0-1)
        reg = getRegisterFromString(args[0]);
    } catch (std::invalid_argument&) { // try as addr/offset/imm8/imm16
        switch (args[0][0]) {
            case '@': { // try as addr (4)
                u16 addr = std::stoul(args[0].substr(1));
                memory[instIndex++] = 4; // MOD byte
                memory[instIndex++] = addr & 0x00FF;
                memory[instIndex++] = (addr & 0xFF00) >> 8;
                return;
            }
            case '[': { // try as offset (5)
                // check for open and close bracket
                // must have at least [RG-n]
                if (args[0].size() < 6) throw std::invalid_argument("Invalid offset for push.");
                if (args[0][0] != '[') throw std::invalid_argument("Invalid offset for push.");
                if (*args[0].rbegin() != ']') throw std::invalid_argument("Invalid offset for push.");
                if (args[0][3] != '-' && args[0][3] != '+') throw std::invalid_argument("Invalid offset for push.");

                // get register
                Register refReg = getRegisterFromString(args[0].substr(1, 2));
                if (refReg != Register::SP && refReg != Register::BP && refReg != Register::CP)
                    throw std::invalid_argument("Invalid register for mov.");

                // extract offset
                int offset = std::stoi(args[0].substr(3, args[0].size() - 4));
                if (offset < -0x8000 || offset > 0x7FFF) throw std::invalid_argument("Expected signed 16-bit literal.");

                memory[instIndex++] = 5; // MOD byte
                memory[instIndex++] = refReg;
                memory[instIndex++] = (u8)(offset & 0x00FF);
                memory[instIndex++] = (u8)((offset & 0xFF00) >> 8);
                return;
            }
            default: { // try as imm8/16 (2-3)
                try {
                    if (isPUSHW) { // try as imm16 (3)
                        u32 arg = std::stoul(args[0]);
                        if (arg > 0xFFFF) throw std::invalid_argument("Expected 16-bit literal.");
                        memory[instIndex++] = 3; // MOD byte
                        memory[instIndex++] = arg & 0x00FF;
                        memory[instIndex++] = (arg & 0xFF00) >> 8;
                    } else { // try as imm8 (2)
                        u16 arg = std::stoul(args[0]);
                        if (arg > 0xFF) throw std::invalid_argument("Expected 8-bit literal.");
                        memory[instIndex++] = 2; // MOD byte
                        memory[instIndex++] = arg;
                    }
                } catch (std::invalid_argument&) {
                    // if this is a label, handle that (only allow u16s)
                    if (!isPUSHW)
                        throw std::invalid_argument("Cannot use u16 value in 8-bit operation.");

                    // make space for value
                    memory[instIndex++] = isPUSHW ? 3 : 2;
                    u16 replaceAddr = instIndex;
                    instIndex += 2;
                    labelsToReplace.push_back({args[0], replaceAddr});
                }
                return;
            }
        }
    }

    // base case, push from register
    if (isPUSHW) { // try as 1
        if (isRegister8Bit(reg)) throw std::invalid_argument("Expected 16-bit register.");
        memory[instIndex++] = 1; // MOD byte
    } else { // try as 0
        if (!isRegister8Bit(reg)) throw std::invalid_argument("Expected 8-bit register.");
        memory[instIndex++] = 0; // MOD byte
    }
    memory[instIndex++] = reg;
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

void parsePOPW(const std::vector<std::string>& args, Memory& memory, u16& instIndex) {
    memory[instIndex++] = OPCode::POPW;
    memory[instIndex++] = 1 - args.size(); // MOD byte

    if (args.size() == 1) { // verify argument is reg16
        Register reg = getRegisterFromString(args[0]);
        if (isRegister8Bit(reg))
            throw std::invalid_argument("Expected 16-bit register.");
        memory[instIndex++] = reg;
    }
}

void parseBitShifts(const std::vector<std::string>& args, Memory& memory, u16& instIndex, bool isLeftShift, bool isSignedOp) {
    memory[instIndex++] = isLeftShift ? OPCode::SHL : OPCode::SHR;

    // determine MOD byte
    Register reg = getRegisterFromString(args[0]);
    u8 MOD = isSignedOp ? 8 : 0;
    MOD |= !isRegister8Bit(reg);

    // get register
    size_t modByteAddr = instIndex++;
    memory[modByteAddr] = MOD; // MOD byte
    memory[instIndex++] = reg;

    // try as register
    try {
        Register regB = getRegisterFromString(args[1]);
        memory[modByteAddr] = memory[modByteAddr].getValue() + 2; // update MOD byte
        if (!isRegister8Bit(regB))
            throw std::runtime_error("Expected 8-bit register.");
        memory[instIndex++] = regB;
    } catch (std::invalid_argument&) { // get imm8
        u16 arg = std::stoul(args[1]);
        if (arg > 0xFF) throw std::invalid_argument("Expected 8-bit literal.");
        memory[instIndex++] = (u8)arg;
    }
}