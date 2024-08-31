#ifndef __TPU_HPP
#define __TPU_HPP

#include "memory.hpp"

// flag macros
// ref: https://www.geeksforgeeks.org/flag-register-8086-microprocessor/?ref=lbp
#define CARRY 0 // 1 if a carry/borrow bit is used during an arithmetic operation
#define PARITY 2 // 1 if the result of arithmetic or logical operation has odd parity
#define ZERO 6 // 1 if the result of arithmetic or logical operation is zero
#define SIGN 7 // 1 if the result of arithmetic or logical operation is negative
#define OVERFLOW 11 // 1 if the result of arithmetic operation overflows/underflows

// instruction set opcodes
enum OPCode {
    NOP         = 0x00,
    HLT         = 0x01,
    SYSCALL     = 0x02,
    JMP         = 0x04,
    MOV         = 0x05,
    ADD         = 0x14,
    SUB         = 0x15,
    MUL         = 0x16,
    DIV         = 0x17,
    AND         = 0x20,
    OR          = 0x21,
    XOR         = 0x22
};

// register codes
enum Register {
    AX      = 0x00,     AL      = 0x01,     AH      = 0x02,
    BX      = 0x03,     BL      = 0x04,     BH      = 0x05,
    CX      = 0x06,     CL      = 0x07,     CH      = 0x08,
    DX      = 0x09,     DL      = 0x0A,     DH      = 0x0B,
    SP      = 0x0C,
    BP      = 0x0D,
    SI      = 0x0E,
    DI      = 0x0F,
    IP      = 0x10,
    FLAGS   = 0x11
};

constexpr Register getRegister16FromCode(unsigned short code) {
    switch (code) {
        case AX: return AX;
        case BX: return BX;
        case CX: return CX;
        case DX: return DX;
        case SP: return SP;
        case BP: return BP;
        case SI: return SI;
        case DI: return DI;
        case IP: return IP;
        case FLAGS: return FLAGS;
        default: throw std::invalid_argument("OPCode does not belong to a 16-bit register: " + code);
    }
}

constexpr Register getRegister8FromCode(unsigned short code) {
    switch (code) {
        case AL: return AL;  case AH: return AH;
        case BL: return BL;  case BH: return BH;
        case CL: return CL;  case CH: return CH;
        case DL: return DL;  case DH: return DH;
        default: throw std::invalid_argument("OPCode does not belong to an 8-bit register: " + code);
    }
}

// the memory module is a continuous max of 64KiB, meaning this emulation does NOT
// handle segmented memory blocks and thus does not use segment registers

class TPU {
    public:
        TPU(int clockFreq) : clockFreq(clockFreq) {};
        ~TPU() { this->reset(); };

        // general purpose registers
        // ref: https://www.geeksforgeeks.org/general-purpose-registers-8086-microprocessor/
        Word AX; // accumulator
        Word BX; // base
        Word CX; // counter
        Word DX; // data
        Word SP; // stack pointer
        Word BP; // base pointer
        Word SI; // source index
        Word DI; // destination index
        Word IP; // instruction pointer/program counter

        // flag register
        Word FLAGS;

        // methods
        void reset();
        void execute(Memory&);
        void start(Memory&); // for starting/running the clock
        void sleep() const;
        void setFlag(u8, bool);

        // helpers
        Byte readByte(Memory&);
        Word readWord(Memory&);
        void moveToRegister(Register, unsigned short);
        const Word& readRegister16(Register) const;
        const Byte& readRegister8(Register) const;
        void syscall();
    private:
        int clockFreq;
        bool __hasSuspended = false; // true when a halt instruction is met
};

#endif