#include <chrono>
#include <thread>

#include "tpu.hpp"
#include "instructions.hpp"

void TPU::reset() {
    // clear registers
    AX = BX = CX = DX = BP = SI = DI = 0x0;

    // fix instruction ptr and stack ptr
    IP = 0x0;
    SP = 0xFFFF; // starts at the end of addressable memory, grows downwards

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
        case Register::FLAGS: FLAGS = value; break;
        default: throw std::invalid_argument("Invalid register for move: " + reg);
    }
}

const Word& TPU::readRegister16(Register reg) const {
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
        case Register::FLAGS: return FLAGS;
        default: throw std::invalid_argument("Invalid 16-bit register for get: " + reg);
    }
}

const Byte& TPU::readRegister8(Register reg) const {
    switch (reg) {
        case Register::AL: return AX.getLower();
        case Register::AH: return AX.getUpper();
        case Register::BL: return BX.getLower();
        case Register::BH: return BX.getUpper();
        case Register::CL: return CX.getLower();
        case Register::CH: return CX.getUpper();
        case Register::DL: return DX.getLower();
        case Register::DH: return DX.getUpper();
        default: throw std::invalid_argument("Invalid 8-bit register for get: " + reg);
    }
}

void TPU::execute(Memory& memory) {
    // fetch instruction
    Byte instruction = this->readByte(memory);

    // wait cycle since TPU has to process the instruction
    this->sleep();

    // switch on instruction
    unsigned short opCode = instruction.getValue();
    switch (opCode) {
        case OPCode::HLT: {
            this->__hasSuspended = true; // trigger clock suspension
            break;
        }
        case OPCode::SYSCALL: {
            this->syscall();
            break;
        }
        case OPCode::NOP: break;
        case OPCode::MOV: {
            instructions::processMOV(*this, memory);
            this->sleep(); // wait since TPU has just completed an operation
            break;
        }
        case OPCode::ADD: {
            instructions::processADD(*this, memory);
            this->sleep(); // wait since TPU has just completed an operation
            break;
        }
        case OPCode::SUB: {
            instructions::processSUB(*this, memory);
            this->sleep(); // wait since TPU has just completed an operation
            break;
        }
        case OPCode::MUL: {
            instructions::processMUL(*this, memory);
            this->sleep(); // wait since TPU has just completed an operation
            break;
        }
        case OPCode::DIV: {
            instructions::processDIV(*this, memory);
            this->sleep(); // wait since TPU has just completed an operation
            break;
        }
        case OPCode::AND: {
            instructions::processAND(*this, memory);
            this->sleep(); // wait since TPU has just completed an operation
            break;
        }
        case OPCode::OR: {
            instructions::processOR(*this, memory);
            this->sleep(); // wait since TPU has just completed an operation
            break;
        }
        case OPCode::XOR: {
            instructions::processXOR(*this, memory);
            this->sleep(); // wait since TPU has just completed an operation
            break;
        }
        default:
            throw std::invalid_argument("Invalid or unimplemented instruction code: " + opCode);
    }
}

// execute a syscall, switching on the value in AX
void TPU::syscall() {
    // wait because TPU has to process its current state
    this->sleep();

    // switch on AX register value
    switch (AX.getValue()) {
        
    }
}

// starts the clock and runs until a halt instruction is encountered
void TPU::start(Memory& memory) {
    while ( !this->__hasSuspended ) {
        this->execute(memory); // execute next instruction
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