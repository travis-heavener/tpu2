#include <chrono>
#include <thread>
#include <iostream>
#include <string>

#include "tpu.hpp"
#include "instructions.hpp"

Register getRegisterFromString(const std::string& str) {
    if (str == "AX") return Register::AX;
    else if (str == "AL") return Register::AL;
    else if (str == "AH") return Register::AH;
    else if (str == "BX") return Register::BX;
    else if (str == "BL") return Register::BL;
    else if (str == "BH") return Register::BH;
    else if (str == "CX") return Register::CX;
    else if (str == "CL") return Register::CL;
    else if (str == "CH") return Register::CH;
    else if (str == "DX") return Register::DX;
    else if (str == "DL") return Register::DL;
    else if (str == "DH") return Register::DH;
    else if (str == "SP") return Register::SP;
    else if (str == "BP") return Register::BP;
    else if (str == "SI") return Register::SI;
    else if (str == "DI") return Register::DI;
    // else if (str == "IP") return Register::IP; // IGNORE THESE TO PREVENT USER INPUTTING THEM
    else if (str == "CP") return Register::CP;
    // else if (str == "FLAGS") return Register::FLAGS; // IGNORE THESE TO PREVENT USER INPUTTING THEM
    else throw std::invalid_argument("Invalid register name: " + str);
}

void TPU::reset() {
    // clear registers
    AX = BX = CX = DX = BP = SI = DI = 0x0;

    // fix instruction ptr and stack ptr
    IP = INSTRUCTION_PTR_START;
    SP = STACK_LOWER_ADDR; // grows upwards (away from reserved pool)
    CP = CALLSTACK_LOWER_ADDR; // grows upwards (in reserved pool)

    // clear flags
    FLAGS = 0x0;

    // reset halt flag
    __hasSuspended = false;
}

Byte TPU::readByte(Memory& memory) {
    return memory[this->IP++];
}

Word TPU::readWord(Memory& memory) {
    // little-endian (lower first, upper second)
    unsigned short value = memory[this->IP++].getValue();
    value |= ((u16)memory[this->IP++].getValue()) << 8;
    return Word(value);
}

void TPU::moveToRegister(Register reg, unsigned short value) {
    switch (reg) {
        case Register::AX: AX = value; break;
        case Register::AL: AX.setLower( value & 0xFF ); break;
        case Register::AH: AX.setUpper( value & 0xFF ); break;
        
        case Register::BX: BX = value; break;
        case Register::BL: BX.setLower( value & 0xFF ); break;
        case Register::BH: BX.setUpper( value & 0xFF ); break;
        
        case Register::CX: CX = value; break;
        case Register::CL: CX.setLower( value & 0xFF ); break;
        case Register::CH: CX.setUpper( value & 0xFF ); break;
        
        case Register::DX: DX = value; break;
        case Register::DL: DX.setLower( value & 0xFF ); break;
        case Register::DH: DX.setUpper( value & 0xFF ); break;
        
        case Register::SP: SP = value; break;
        case Register::BP: BP = value; break;
        case Register::SI: SI = value; break;
        case Register::DI: DI = value; break;
        case Register::IP: IP = value; break;
        case Register::CP: CP = value; break;
        case Register::FLAGS: FLAGS = value; break;
        default: throw std::invalid_argument("Invalid register for move: " + reg);
    }
}

Word& TPU::readRegister16(Register reg) {
    switch (reg) {
        case Register::AX: return AX;
        case Register::BX: return BX;
        case Register::CX: return CX;
        case Register::DX: return DX;
        case Register::SP: return SP;
        case Register::BP: return BP;
        case Register::SI: return SI;
        case Register::DI: return DI;
        case Register::IP: return IP;
        case Register::CP: return CP;
        case Register::ES: return ES;
        case Register::FLAGS: return FLAGS;
        default: throw std::invalid_argument("Invalid 16-bit register for get: " + reg);
    }
}

Byte& TPU::readRegister8(Register reg) {
    switch (reg) {
        case Register::AL: return (Byte&)AX.getLower();
        case Register::AH: return (Byte&)AX.getUpper();
        case Register::BL: return (Byte&)BX.getLower();
        case Register::BH: return (Byte&)BX.getUpper();
        case Register::CL: return (Byte&)CX.getLower();
        case Register::CH: return (Byte&)CX.getUpper();
        case Register::DL: return (Byte&)DX.getLower();
        case Register::DH: return (Byte&)DX.getUpper();
        default: throw std::invalid_argument("Invalid 8-bit register for get: " + reg);
    }
}

void TPU::execute(Memory& memory) {
    // fetch instruction
    Byte instruction = this->readByte(memory);

    // wait cycle since TPU has to process the instruction
    this->sleep();

    #define caseInstruction(INST) case OPCode::INST: { \
        instructions::process##INST(*this, memory); \
        this->sleep(); \
        break; \
    }

    // switch on instruction
    unsigned short opCode = instruction.getValue();
    switch (opCode) {
        case OPCode::NOP: break;
        case OPCode::HLT: {
            this->__hasSuspended = true; // trigger clock suspension
            break;
        }
        case OPCode::SYSCALL: {
            instructions::executeSyscall(*this, memory);
            this->sleep(); // wait since the TPU has just completed a syscall
            break;
        }
        caseInstruction(CALL)
        caseInstruction(RET)
        caseInstruction(JMP)
        caseInstruction(MOV)
        caseInstruction(MOVW)
        caseInstruction(PUSH)
        caseInstruction(POP)
        caseInstruction(POPW)
        caseInstruction(ADD)
        caseInstruction(SUB)
        caseInstruction(MUL)
        caseInstruction(DIV)
        caseInstruction(BUF)
        caseInstruction(AND)
        caseInstruction(OR)
        caseInstruction(XOR)
        caseInstruction(NOT)
        caseInstruction(SHL)
        caseInstruction(SHR)
        default:
            throw std::invalid_argument("Invalid or unimplemented instruction code: " + opCode);
    }

    // verify the SP is in bounds
    if (SP.getValue() < STACK_LOWER_ADDR || SP.getValue() > STACK_UPPER_ADDR) {
        throw std::runtime_error("Stack over/underflow");
    }
}

// starts the clock and runs until a halt instruction is encountered
void TPU::start(Memory& memory) {
    while ( !this->__hasSuspended ) {
        // execute next instruction
        this->execute(memory);
    }
}

// thread sleep between cycles
void TPU::sleep() const {
    const long sleepTime = 1e+6 / this->clockFreq;
    std::this_thread::sleep_for(std::chrono::microseconds( sleepTime ));
}

// update a specific flag
void TPU::setFlag(u8 flag, bool isSet) {
    if (isSet) {
        FLAGS.setValue( FLAGS.getValue() | (1u << flag) );
    } else {
        FLAGS.setValue( FLAGS.getValue() & ~(1u << flag) );
    }
}