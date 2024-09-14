#ifndef __TPU_HPP
#define __TPU_HPP

#include "memory.hpp"


/**
 * |------------------------------------ MEMORY MAP ------------------------------------|
 * | 0x0-0x07FF | 0x0800-0x0FFF | 0x1000-0x1FFF |             0x1400-0xFFFF             |
 * | (RESERVED) |  (CALLSTACK)  |    (STACK)    |                (FREE)                 |
 * |------------------------------------------------------------------------------------|
*/

// allocate 2KiB for OS (-4 bytes for instruction ptr start)
#define RESERVED_LOWER_ADDR   0x0000
#define RESERVED_UPPER_ADDR   0x07FB

// allocate 2KiB for callstack
#define CALLSTACK_LOWER_ADDR  0x0800
#define CALLSTACK_UPPER_ADDR  0x0FFF

// allocate 4KiB for stack
#define STACK_LOWER_ADDR      0x1000
#define STACK_UPPER_ADDR      0x1FFF

// set instruction pointer start at end of reserved
#define INSTRUCTION_PTR_START RESERVED_UPPER_ADDR+1 // needs 4 bytes (JMP opcode, MOD byte, lower-addr, upper-addr)

// allocate remaining 56KiB for free use
#define FREE_LOWER_ADDR       0x2000
#define FREE_UPPER_ADDR       0xFFFF

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
    CALL        = 0x03,
    RET         = 0x04,
    JMP         = 0x05,
    MOV         = 0x06,
    MOVW        = 0x07,
    PUSH        = 0x08,
    POP         = 0x09,
    POPW        = 0x0A,
    ADD         = 0x14,
    SUB         = 0x15,
    MUL         = 0x16,
    DIV         = 0x17,
    BUF         = 0x1F,
    AND         = 0x20,
    OR          = 0x21,
    XOR         = 0x22,
    NOT         = 0x23,
    SHL         = 0x24,
    SHR         = 0x25
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
    CP      = 0x11,
    ES      = 0x12,
    FLAGS   = 0x13
};

// syscall codes
enum Syscall {
    STDOUT      = 0x00,     STDERR      = 0x01,     STDIN       = 0x02,
    EXIT_STATUS = 0x03
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
        case CP: return CP;
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

Register getRegisterFromString(const std::string&);

constexpr bool isRegister8Bit(Register reg) {
    return reg == Register::AL || reg == Register::AH || reg == Register::BL || reg == Register::BH || reg == Register::CL || reg == Register::CH || reg == Register::DL || reg == Register::DH;
}

// the memory module is a continuous max of 64KiB, meaning this emulation does NOT
// handle segmented memory blocks and thus does not use segment registers

class TPU {
    public:
        TPU(int clockFreq) : clockFreq(clockFreq) { this->reset(); };
        ~TPU() { this->reset(); };

        // general purpose registers
        // ref: https://www.geeksforgeeks.org/general-purpose-registers-8086-microprocessor/
        Word AX; // accumulator
        Word BX; // base
        Word CX; // counter
        Word DX; // data
        Word SP; // stack pointer
        Word BP; // base pointer
        Word CP; // call stack pointer
        Word SI; // source index
        Word DI; // destination index
        Word IP; // instruction pointer/program counter
        Word ES; // the exit status of the last executed program

        // flag register
        Word FLAGS;

        // methods
        void reset();
        void execute(Memory&);
        void start(Memory&); // for starting/running the clock
        void sleep() const;
        bool getFlag(u8 flag) const { return (FLAGS.getValue() & (1u << flag)) > 0; };
        void setFlag(u8, bool);

        // helpers
        Byte readByte(Memory&);
        Word readWord(Memory&);
        void moveToRegister(Register, unsigned short);
        Word& readRegister16(Register);
        Byte& readRegister8(Register);
        void setExitCode(u16 code) { this->ES = code; };
    private:
        int clockFreq;
        bool __hasSuspended = false; // true when a halt instruction is met
};

#endif