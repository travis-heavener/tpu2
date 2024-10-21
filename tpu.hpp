#ifndef __TPU_HPP
#define __TPU_HPP

#include "util/globals.hpp"
#include "memory.hpp"

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

        u8 addressingMode;

        // flag register
        Word FLAGS;

        // methods
        void reset();
        void execute(Memory&);
        void start(Memory&); // for starting/running the clock
        void sleep(int=1) const;
        bool getFlag(u8 flag) const { return (FLAGS.getValue() & (1u << flag)) > 0; };
        void setFlag(u8, bool);
        u8 getAddressingMode() const { return addressingMode; };
        void setAddressingMode(u8 a) { addressingMode = a; };
        u16 getProgramStartIndex(Memory&) const;

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