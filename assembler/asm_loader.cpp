#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "../util/globals.hpp"
#include "asm_loader.hpp"
#include "../memory.hpp"

// abstractions from processLineToText for readability
void parseMOV(const std::vector<std::string>&, Memory&, u16&, std::vector<std::pair<std::string, u16>>&);
void parseADDSUBLogic(const std::vector<std::string>&, Memory&, u16&, OPCode, bool);
void parseMULDIVBUF(const std::vector<std::string>&, Memory&, u16&, OPCode, bool);
void parseNOT(const std::vector<std::string>&, Memory&, u16&);
void parsePUSH(const std::vector<std::string>&, Memory&, u16&, bool, std::vector<std::pair<std::string, u16>>&);
void parsePOP(const std::vector<std::string>&, Memory&, u16&, bool);

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
u16 loadFileToMemory(const std::string& path, Memory& memory) {
    // open file
    std::ifstream inHandle(path);

    if (!inHandle.is_open()) {
        std::cerr << "Failed to open file: " + path << std::endl;
        exit(1);
    }

    // read each line
    u16 memIndex = 0;
    std::string line;
    label_map_t labelMap; // label name, start address
    
    // for labels that come after instIndex
    std::vector<std::pair<std::string, u16>> labelsToReplace; // [ label name, replacement start address ]

    // allocate space at the start of .text to jump to the main entry point
    memory[memIndex++] = OPCode::JMP;
    memory[memIndex++] = 0; // MOD byte
    labelsToReplace.push_back({RESERVED_LABEL_MAIN, memIndex}); // add this jmp instruction to labelsToReplace
    memIndex += 2; // make space for address

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
                    processLineToText(line, memory, memIndex, labelMap, labelsToReplace); // process the line
                    break;
                case SECTION_DATA:
                    processLineToData(line, memory, memIndex, labelMap); // process the line
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
            if (label.type == DATA_TYPE_DEFAULT || label.type == DATA_TYPE_STRZ) {
                // replace with address
                u16 destAddr = labelMap[labelPair.first].value;
                u16 addr = labelPair.second;
                memory[ addr ] = destAddr & 0x00FF;
                memory[addr+1] = (destAddr & 0xFF00) >> 8;
            } else {
                throw std::invalid_argument("Invalid label type: " + labelPair.first);
            }
        }

        // close file
        inHandle.close();
    } catch (std::invalid_argument& e) {
        // close inHandle
        inHandle.close();
        throw e;
    }

    // return the size taken by the binary file
    return memIndex;
}

// process an individual line from .data section and load it into memory
void processLineToData(std::string& line, Memory& memory, u16& memIndex, label_map_t& labelMap) {
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
    if (dataType == DATA_TYPE_STRZ) { // parse as string
        // verify string is valid
        if (!isStringValid(rawValue))
            throw std::invalid_argument("Invalid string in data declaration.");

        // extract raw value
        rawValue = rawValue.substr(1, rawValue.size()-2);
        escapeString(rawValue);

        // insert each character onto the document
        u16 startIndex = memIndex;
        for (const char c : rawValue) {
            memory[memIndex++] = (u8)c;
        }

        // add null terminator
        memory[memIndex++] = '\0';

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

        std::vector<u8> bytesToWrite;
        u8 status = resolveArgument(args[0], bytesToWrite);
        switch (status) {
            case ARG_LABEL: {
                // get address of label from map
                if (labelMap.count(args[0]) == 0) {
                    u16 startAddr = instIndex;
                    labelsToReplace.push_back({args[0], startAddr});
                    bytesToWrite.push_back(0); // add placeholder byte
                    bytesToWrite.push_back(0); // add placeholder byte
                } else {
                    u16 destAddr = labelMap[args[0]].value;
                    bytesToWrite.push_back( destAddr & 0xFF ); // lower-half
                    bytesToWrite.push_back( (destAddr >> 8) & 0xFF ); // upper-half
                }
                break;
            }
            case ARG_IMM16: break; // already pushed to bytesToWrite
            default: throw std::invalid_argument("Invalid argument to call.");
        }

        // write bytes
        for (u8 b : bytesToWrite) memory[instIndex++] = b;
    } else if (kwd == "ret") {
        checkArgs(args, 0); // check for extra args
        memory[instIndex++] = OPCode::RET;
    } else if (kwd == "jmp" || kwd == "jz" || kwd == "jnz" || kwd == "jc" || kwd == "jnc") {
        checkArgs(args, 1); // check for extra args
        memory[instIndex++] = OPCode::JMP;
        memory[instIndex++] = kwd == "jmp" ? 0 : kwd == "jz" ? 1 : kwd == "jnz" ? 2 : kwd == "jc" ? 3 : 4; // MOD byte

        std::vector<u8> bytesToWrite;
        u8 status = resolveArgument(args[0], bytesToWrite);
        switch (status) {
            case ARG_LABEL: {
                // get address of label from map
                if (labelMap.count(args[0]) == 0) {
                    u16 startAddr = instIndex;
                    labelsToReplace.push_back({args[0], startAddr});
                    bytesToWrite.push_back(0); // add placeholder byte
                    bytesToWrite.push_back(0); // add placeholder byte
                } else {
                    u16 destAddr = labelMap[args[0]].value;
                    bytesToWrite.push_back( destAddr & 0xFF ); // lower-half
                    bytesToWrite.push_back( (destAddr >> 8) & 0xFF ); // upper-half
                }
                break;
            }
            case ARG_IMM16: break; // already pushed to bytesToWrite
            default: throw std::invalid_argument("Invalid argument to jmp.");
        }

        // write bytes
        for (u8 b : bytesToWrite) memory[instIndex++] = b;
    } else if (kwd == "mov") {
        checkArgs(args, 2); // check for extra args
        parseMOV(args, memory, instIndex, labelsToReplace);
    } else if (kwd == "push" || kwd == "pushw") {
        checkArgs(args, 1); // check for extra args
        parsePUSH(args, memory, instIndex, kwd == "pushw", labelsToReplace);
    } else if (kwd == "pop" || kwd == "popw") {
        if (args.size() > 1) throw std::invalid_argument("Invalid number of arguments.");
        parsePOP(args, memory, instIndex, kwd == "popw");
    } else if (kwd == "add" || kwd == "sub" || kwd == "sadd" || kwd == "ssub" || kwd == "and" || kwd == "or" || kwd == "xor" || kwd == "cmp" || kwd == "scmp" || kwd == "shl" || kwd == "shr" || kwd == "sshl" || kwd == "sshr") {
        checkArgs(args, 2); // check for extra args
        OPCode code = (kwd == "add" || kwd == "sadd") ? OPCode::ADD : (kwd == "ssub" || kwd == "sub") ? OPCode::SUB :
                      kwd == "and" ? OPCode::AND : kwd == "or" ? OPCode::OR : (kwd == "cmp" || kwd == "scmp") ? OPCode::CMP :
                      (kwd == "shl" || kwd == "sshl") ? OPCode::SHL : (kwd == "shr" || kwd == "sshr") ? OPCode::SHR : OPCode::XOR;
        bool isSignedOp = kwd == "sadd" || kwd == "ssub" || kwd == "scmp" || kwd == "sshl" || kwd == "sshr";
        parseADDSUBLogic(args, memory, instIndex, code, isSignedOp);
    } else if (kwd == "mul" || kwd == "div" || kwd == "smul" || kwd == "sdiv" || kwd == "buf") {
        checkArgs(args, 1); // check for extra args
        bool isSignedOp = kwd == "smul" || kwd == "sdiv";
        OPCode opCode = kwd == "buf" ? OPCode::BUF : (kwd == "mul" || kwd == "smul") ? OPCode::MUL : OPCode::DIV;
        parseMULDIVBUF(args, memory, instIndex, opCode, isSignedOp);
    } else if (kwd == "not") {
        checkArgs(args, 1); // check for extra args
        parseNOT(args, memory, instIndex);
    } else if (kwd.back() == ':') { // label name
        checkArgs(args, 0); // verify rest of line is empty
        std::string labelName = kwd.substr(0, kwd.size()-1);

        // verify label name is valid
        if (kwd.size() == 1) throw std::invalid_argument("Invalid label name: " + kwd);

        // verify the arg is a label
        std::vector<u8> bytesToWrite;
        u8 status = resolveArgument(labelName, bytesToWrite);
        if (status != ARG_LABEL) throw std::invalid_argument("Invalid label name: " + labelName);

        labelMap[labelName].value = instIndex; // store entry point
    } else {
        // invalid instruction
        throw std::invalid_argument("Invalid instruction: " + kwd);
    }
}

// abstraction to parse a MOV instruction
void parseMOV(const std::vector<std::string>& args, Memory& memory, u16& instIndex, std::vector<std::pair<std::string, u16>>& labelsToReplace) {
    memory[instIndex++] = OPCode::MOV;

    // determine MOD byte
    std::vector<u8> bytesToWrite;
    u8 MOD = 0;

    // determine arg A
    u8 statusA = resolveArgument(args[0], bytesToWrite);
    switch (statusA) {
        case ARG_REG8: case ARG_REG16: case ARG_ADDR_DIRECT: break;
        case ARG_ADDR_OFFSET: {
            MOD |= 16; // set 5th bit to mark this as an offset address
            break;
        }
        default: throw std::invalid_argument("Invalid first argument to mov.");
    }

    // determine arg B
    u8 statusB = resolveArgument(args[1], bytesToWrite, statusA == ARG_REG16, true);
    switch (statusB) {
        case ARG_REG8: {
            if (statusA == ARG_REG8)
                MOD |= 4;
            else if (statusA == ARG_ADDR_DIRECT || statusA == ARG_ADDR_OFFSET)
                MOD |= 1;
            else
                throw std::invalid_argument("Invalid second argument to mov.");
            break;
        }
        case ARG_REG16: {
            if (statusA != ARG_REG16)
                throw std::invalid_argument("Invalid second argument to mov.");
            MOD |= 6;
            break;
        }
        case ARG_IMM8: {
            if (statusA == ARG_REG8)
                MOD |= 2;
            else if (statusA == ARG_ADDR_DIRECT || statusA == ARG_ADDR_OFFSET)
                MOD |= 0;
            else
                throw std::invalid_argument("Invalid second argument to mov.");
            break;
        }
        case ARG_IMM16: {
            if (statusA != ARG_REG16)
                throw std::invalid_argument("Invalid second argument to mov.");
            MOD |= 5;
            break;
        }
        case ARG_LABEL: {
            if (statusA != ARG_REG16)
                throw std::invalid_argument("Invalid second argument to mov.");
            MOD |= 5;

            // add to labels to replace
            u16 labelAddr = instIndex + 2; // skip MOD byte & reg16
            labelsToReplace.push_back({args[1], labelAddr});
            bytesToWrite.push_back(0); // add placeholder bytes
            bytesToWrite.push_back(0); // add placeholder bytes
            break;
        }
        case ARG_ADDR_DIRECT: {
            if (statusA != ARG_REG8)
                throw std::invalid_argument("Invalid second argument to mov.");
            MOD |= 3;
            break;
        }
        case ARG_ADDR_OFFSET: {
            if (statusA != ARG_REG8)
                throw std::invalid_argument("Invalid second argument to mov.");
            MOD |= 3 | 32; // set MOD byte to 3 and set the 6th bit to mark this as an offset address
            break;
        }
        default: throw std::invalid_argument("Invalid second argument to mov.");
    }

    // write bytes
    memory[instIndex++] = MOD;
    for (u8 b : bytesToWrite) memory[instIndex++] = b;
}

// most arithmetic operations follow this convention
void parseADDSUBLogic(const std::vector<std::string>& args, Memory& memory, u16& instIndex, OPCode instruction, bool isSignedOp) {
    // extract args
    std::vector<u8> bytesToWrite;
    u8 MOD = isSignedOp ? 8 : 0;
    u8 statusA = resolveArgument(args[0], bytesToWrite);

    // only allow register as first operand
    if (statusA != ARG_REG8 && statusA != ARG_REG16)
        throw std::invalid_argument("Invalid first argument to arith/logic.");

    u8 statusB = resolveArgument(args[1], bytesToWrite, statusA == ARG_REG16, isSignedOp);
    if (statusB != ARG_REG8 && statusB != ARG_REG16 && statusB != ARG_IMM8 && statusB != ARG_IMM16)
        throw std::invalid_argument("Invalid second argument to arith/logic.");

    // allow 16-bit and 8-bit mismatch for shl/shr
    if ((instruction == OPCode::SHL || instruction == OPCode::SHR) && statusA == ARG_REG16 && statusB == ARG_REG8) {
        statusB = ARG_REG16; // doesn't change any functionality, just prevents exception throwing
        bytesToWrite.push_back(0);
    }

    // determine MOD byte
    if (statusA == ARG_REG8) {
        if      (statusB == ARG_REG8)   MOD |= 2;
        else if (statusB == ARG_IMM8)   MOD |= 0;
        else throw std::invalid_argument("8-bit and 16-bit register mismatch in arith/logic.");
    } else {
        if      (statusB == ARG_REG16)  MOD |= 3;
        else if (statusB == ARG_IMM16)  MOD |= 1;
        else throw std::invalid_argument("8-bit and 16-bit register mismatch in arith/logic.");
    }

    // write bytes
    memory[instIndex++] = instruction;
    memory[instIndex++] = MOD;
    for (u8 b : bytesToWrite) memory[instIndex++] = b;
}

// MUL, DIV, and BUF use the convention
void parseMULDIVBUF(const std::vector<std::string>& args, Memory& memory, u16& instIndex, OPCode op, bool isSignedOp) {
    // extract args
    std::vector<u8> bytesToWrite;
    u8 MOD = isSignedOp ? 8 : 0;
    u8 statusA = resolveArgument(args[0], bytesToWrite, isSignedOp);

    // only allow register as first operand
    switch (statusA) {
        case ARG_IMM8:  MOD |= 0; break;
        case ARG_IMM16: MOD |= 1; break;
        case ARG_REG8:  MOD |= 2; break;
        case ARG_REG16: MOD |= 3; break;
        default: throw std::invalid_argument("Invalid first argument to mul/div/buf.");
    }

    // write bytes
    memory[instIndex++] = op;
    memory[instIndex++] = MOD;
    for (u8 b : bytesToWrite) memory[instIndex++] = b;
}

void parseNOT(const std::vector<std::string>& args, Memory& memory, u16& instIndex) {
    // extract args
    std::vector<u8> bytesToWrite;
    u8 MOD = 0;
    u8 statusA = resolveArgument(args[0], bytesToWrite);

    // only allow register as first operand
    switch (statusA) {
        case ARG_REG8:  MOD |= 0; break;
        case ARG_REG16: MOD |= 1; break;
        default: throw std::invalid_argument("Invalid first argument to not.");
    }

    // write bytes
    memory[instIndex++] = OPCode::NOT;
    memory[instIndex++] = MOD;
    for (u8 b : bytesToWrite) memory[instIndex++] = b;
}

void parsePUSH(const std::vector<std::string>& args, Memory& memory, u16& instIndex, bool isPUSHW, std::vector<std::pair<std::string, u16>>& labelsToReplace) {
    memory[instIndex++] = OPCode::PUSH;

    // extract args
    std::vector<u8> bytesToWrite;
    u8 MOD = 0;
    u8 statusA = resolveArgument(args[0], bytesToWrite, isPUSHW);

    // only allow register as first operand
    switch (statusA) {
        case ARG_REG8:
            if (isPUSHW) throw std::invalid_argument("Invalid 8-bit operation.");
            MOD |= 0;
            break;
        case ARG_REG16:
            if (!isPUSHW) throw std::invalid_argument("Invalid 16-bit operation.");
            MOD |= 1;
            break;
        case ARG_IMM8:
            if (isPUSHW) throw std::invalid_argument("Invalid 8-bit operation.");
            MOD |= 2;
            break;
        case ARG_IMM16:
            if (!isPUSHW) throw std::invalid_argument("Invalid 16-bit operation.");
            MOD |= 3;
            break;
        case ARG_ADDR_DIRECT:
            if (isPUSHW) throw std::invalid_argument("Invalid 8-bit operation.");
            MOD |= 4;
            break;
        case ARG_ADDR_OFFSET: {
            if (isPUSHW) throw std::invalid_argument("Invalid 8-bit operation.");
            MOD |= 4 | 16; // set MOD byte to 4 and set the 5th bit to mark this as an offset address
            break;
        }
        case ARG_LABEL: {
            if (!isPUSHW) throw std::invalid_argument("Invalid 16-bit operation.");
            MOD |= 3;
            // add to labels to replace
            u16 labelAddr = instIndex + 1; // skip MOD byte
            labelsToReplace.push_back({args[0], labelAddr});
            bytesToWrite.push_back(0); // add placeholder bytes
            bytesToWrite.push_back(0); // add placeholder bytes
            break;
        }
        default: throw std::invalid_argument("Invalid first argument to push.");
    }

    // write bytes
    memory[instIndex++] = MOD;
    for (u8 b : bytesToWrite) memory[instIndex++] = b;
}

void parsePOP(const std::vector<std::string>& args, Memory& memory, u16& instIndex, bool isPOPW) {
    memory[instIndex++] = OPCode::POP;

    // check for any args
    std::vector<u8> bytesToWrite;
    u8 MOD = 0;
    if (args.size() == 1) {
        // extract args
        u8 statusA = resolveArgument(args[0], bytesToWrite);

        // only allow register as first operand
        switch (statusA) {
            case ARG_REG8:
                if (isPOPW) throw std::invalid_argument("Invalid 8-bit operation.");
                MOD |= 0;
                break;
            case ARG_REG16:
                if (!isPOPW) throw std::invalid_argument("Invalid 16-bit operation.");
                MOD |= 1;
                break;
            default: throw std::invalid_argument("Invalid first argument to pop.");
        }
    } else {
        MOD |= isPOPW ? 3 : 2;
    }

    // write bytes
    memory[instIndex++] = MOD;
    for (u8 b : bytesToWrite) memory[instIndex++] = b;
}

// used to resolve an argument into its corresponding numeric value
u8 resolveArgument(const std::string& arg, std::vector<u8>& bytesToWrite, const bool forceIMM16, const bool allowSigned) {
    // attempt to regex match the argument
    if (std::regex_match(arg, RE_ARG_REG16)) { // treat as reg16
        bytesToWrite.push_back( getRegisterFromString(arg) );
        return ARG_REG16;
    } else if (std::regex_match(arg, RE_ARG_REG8)) { // treat as reg8
        bytesToWrite.push_back( getRegisterFromString(arg) );
        return ARG_REG8;
    } else if (std::regex_match(arg, RE_ARG_ADDR_DIRECT)) { // treat as direct addr
        u8 radix = (arg[2] == 'b' || arg[2] == 'B') ? 2 : (arg[2] == 'x' || arg[2] == 'X') ? 16 : 10;
        u16 addr = std::stoull(arg, nullptr, radix);
        bytesToWrite.push_back( addr & 0xFF );
        bytesToWrite.push_back( (addr >> 8) & 0xFF );
        return ARG_ADDR_DIRECT;
    } else if (std::regex_match(arg, RE_ARG_ADDR_OFFSET)) { // treat as offset addr
        // extract register
        const std::string offsetStr = arg.substr(0, arg.size()-4);

        // extract offset
        bool isNeg = offsetStr[0] == '-';

        u8 radix = (offsetStr[1+isNeg] == 'b' || offsetStr[1+isNeg] == 'B') ?  2 :
                   (offsetStr[1+isNeg] == 'x' || offsetStr[1+isNeg] == 'X') ? 16 : 10;
        u32 immed = std::stoull(offsetStr.substr(isNeg), nullptr, radix);

        if (isNeg && immed > 0x7FFF) throw std::invalid_argument("Immediate signed value exceeds 16-bits.");
        if (!isNeg && immed > 0xFFFF) throw std::invalid_argument("Immediate value exceeds 16-bits.");

        // handle as signed
        u16 imm16 = 0 | ((isNeg ? -1 : 1) * (s16)immed);

        // push the bytes
        bytesToWrite.push_back( imm16 & 0xFF );
        bytesToWrite.push_back( (imm16 >> 8) & 0xFF );

        // push register code
        bytesToWrite.push_back( getRegisterFromString( arg.substr(arg.size()-3, 2) ) );
        return ARG_ADDR_OFFSET;
    } else if (std::regex_match(arg, RE_ARG_LABEL)) { // treat as label (becomes imm16)
        return ARG_LABEL;
    } else if (std::regex_match(arg, RE_ARG_IMMED)) { // treat as immediate value
        bool isNeg = arg[0] == '-';
        if (isNeg && !allowSigned) throw std::invalid_argument("Cannot use a signed immediate value here.");

        u8 radix = (arg[1+isNeg] == 'b' || arg[1+isNeg] == 'B') ? 2 : (arg[1+isNeg] == 'x' || arg[1+isNeg] == 'X') ? 16 : 10;
        u32 immed = std::stoull(arg.substr(isNeg), nullptr, radix);

        if (isNeg && immed > 0x7FFF) throw std::invalid_argument("Immediate signed value exceeds 16-bits.");
        if (!isNeg && immed > 0xFFFF) throw std::invalid_argument("Immediate value exceeds 16-bits.");

        // handle as signed
        s16 simm16 = (isNeg ? -1 : 1) * (s16)immed;
        u16 imm16 = 0 | simm16;

        // push the bytes
        bytesToWrite.push_back( imm16 & 0xFF );
        bool isImm16 = forceIMM16 || (isNeg && (simm16 > 0x7F || simm16 < -0x80)) || (!isNeg && imm16 > 0xFF);
        if (isImm16) bytesToWrite.push_back( (imm16 >> 8) & 0xFF );
        return isImm16 ? ARG_IMM16 : ARG_IMM8;
    }

    throw std::invalid_argument("Argument could not be parsed.");
}