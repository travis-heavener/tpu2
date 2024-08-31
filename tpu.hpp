#ifndef __TPU_HPP
#define __TPU_HPP

#include "memory.hpp"

// flag macros
// ref: https://www.geeksforgeeks.org/flag-register-8086-microprocessor/?ref=lbp
#define CARRY 0
#define PARITY 2
#define AUX_CARRY 4
#define ZERO 6
#define SIGN 7
#define TRAP 8
#define INTERRUPT 9
#define DIRECTION 10
#define OVERFLOW 11

class TPU {
    public:
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

        // segment registers
        // ref: https://www.geeksforgeeks.org/types-of-registers-in-8086-microprocessor/#special-purpose-registers
        Word CS; // code segment
        Word DS; // data segment
        Word SS; // stack segment
        Word ES; // extra segment

        // flag register
        Word FLAGS;

        // methods
        void reset();
};

#endif