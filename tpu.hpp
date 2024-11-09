#ifndef __TPU_HPP
#define __TPU_HPP

#include "util/globals.hpp"
#include "memory.hpp"

class TPU {
    public:
        TPU(int clockFreq, Memory& memory, Memory& reservedMem) : clockFreq(clockFreq), memory(memory), reservedMem(reservedMem) { this->reset(); };
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
        void execute();
        void start(); // for starting/running the clock
        void sleep(int=1) const;
        bool getFlag(u8 flag) const { return (FLAGS.getValue() & (1u << flag)) > 0; };
        void setFlag(u8, bool);
        u8 getAddressingMode() const { return addressingMode; };
        void setAddressingMode(u8 a) { addressingMode = a; };
        u16 getProgramStartIndex() const;

        // I/O
        Byte readNextByte();
        Word readNextWord();
        Byte readByte(u16) const;
        Word readWord(u16) const;
        Byte readByte(Word w) const { return readByte(w.getValue()); };
        Word readWord(Word w) const { return readWord(w.getValue()); };

        void writeByte(u16, u8) const;
        void writeByte(u16 w, Byte v) const { writeByte(w, v.getValue()); };
        void writeByte(Word w, u8 v) const { writeByte(w.getValue(), v); };
        void writeByte(Word w, Byte v) const { writeByte(w.getValue(), v.getValue()); };
        void writeWord(u16, u16) const;
        void writeWord(u16 w, Word v) const { writeWord(w, v.getValue()); };
        void writeWord(Word w, u16 v) const { writeWord(w.getValue(), v); };
        void writeWord(Word w, Word v) const { writeWord(w.getValue(), v.getValue()); };

        void moveToRegister(Register, u16);
        Word& readRegister16(Register);
        Byte& readRegister8(Register);

        // helpers
        void setExitCode(u16 code) { this->ES = code; };
    private:
        int clockFreq;
        Memory& memory;
        Memory& reservedMem;
        bool __hasSuspended = false; // true when a halt instruction is met
};

#endif