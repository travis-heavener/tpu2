#include <chrono>
#include <thread>
#include <iostream>
#include <string>
#include <random>

#include "tpu.hpp"
#include "instructions.hpp"

void TPU::reset() {
    // clear registers
    AX = BX = CX = DX = BP = SI = DI = 0x0;

    // fix instruction ptr and stack ptr
    IP = RESERVED_LOWER_ADDR;
    SP = STACK_LOWER_ADDR; // grows upwards (away from reserved pool)
    CP = CALLSTACK_LOWER_ADDR; // grows upwards (in reserved pool)

    // clear flags
    FLAGS = 0x0;

    // default addressing mode
    addressingMode = ADDRESS_MODE_ABSOLUTE;

    // reset halt flag
    __hasSuspended = false;
}

Byte TPU::readNextByte() {
    return readByte(this->IP++);
}

Word TPU::readNextWord() {
    Word value = readWord(this->IP);
    this->IP++; this->IP++;
    return value;
}

Byte TPU::readByte(u16 addr) const {
    sleep(CPI_MEM_READ);
    return memory[addr];
}

Word TPU::readWord(u16 addr) const {
    // little-endian (lower first, upper second)
    sleep(2 * CPI_MEM_READ);
    u16 value = memory[addr].getValue();
    value |= ((u16)memory[addr+1].getValue()) << 8;
    return Word(value);
}

void TPU::writeByte(u16 addr, u8 value) const {
    sleep(CPI_MEM_WRITE);
    memory[addr] = value;
}

void TPU::writeWord(u16 addr, u16 value) const {
    sleep(2 * CPI_MEM_WRITE);
    memory[addr] = value & 0xFF;
    memory[addr+1] = (value >> 8) & 0xFF;
}

void TPU::moveToRegister(Register reg, u16 value) {
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
        default: {
            const std::string msg = "Invalid 16-bit register for get: " + std::to_string(reg);
            throw std::invalid_argument(msg);
        }
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
        default: {
            const std::string msg = "Invalid 8-bit register for get: " + std::to_string(reg);
            throw std::invalid_argument(msg);
        }
    }
}

void TPU::execute() {
    // fetch instruction
    Byte instruction = this->readNextByte();

    #define caseInstruction(INST) case OPCode::INST: { \
        instructions::process##INST(*this); \
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
            instructions::executeSyscall(*this);
            break;
        }
        case OPCode::JMP: case OPCode::JZ: case OPCode::JNZ: case OPCode::JC: case OPCode::JNC: {
            instructions::processJMP(*this, opCode);
            break;
        }
        caseInstruction(CALL)
        caseInstruction(RET)
        case OPCode::MOV: case OPCode::MOVW: case OPCode::MOVI: case OPCode::MOVWI: {
            instructions::processMOV(*this, opCode);
            break;
        }
        caseInstruction(LB)
        caseInstruction(LW)
        caseInstruction(SB)
        caseInstruction(SW)
        case OPCode::PUSH: case OPCode::PUSHW: case OPCode::PUSHI:
        case OPCode::PUSHWI: case OPCode::PUSHA: case OPCode::PUSHWA: {
            instructions::processPUSH(*this, opCode);
            break;
        }
        case OPCode::POP: case OPCode::POPW: {
            instructions::processPOP(*this, opCode);
            break;
        }
        caseInstruction(ADD)
        caseInstruction(SUB)
        caseInstruction(MUL)
        caseInstruction(DIV)
        caseInstruction(CMP)
        caseInstruction(BUF)
        case OPCode::AND: case OPCode::OR: case OPCode::XOR: {
            instructions::processANDORXOR(*this, opCode);
            break;
        }
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
void TPU::start() {
    while ( !this->__hasSuspended ) {
        // execute next instruction
        this->execute();
    }
}

// thread sleep between cycles
void TPU::sleep(int cycles) const {
    const long sleepTime = 1e+6 / this->clockFreq;
    for (int i = 0; i < cycles; ++i)
        std::this_thread::sleep_for(std::chrono::microseconds( sleepTime ));
}

u16 TPU::getProgramStartIndex() const {
    return readWord(PROGRAM_INDEX).getValue();
}

// update a specific flag
void TPU::setFlag(u8 flag, bool isSet) {
    if (isSet) {
        FLAGS.setValue( FLAGS.getValue() | (1u << flag) );
    } else {
        FLAGS.setValue( FLAGS.getValue() & ~(1u << flag) );
    }
}