#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
    #include <conio.h>
#else
    #include <curses.h>
#endif
#include <iostream>

#include "instructions.hpp"
#include "tpu.hpp"
#include "memory.hpp"
#include "kernel/kernel.hpp"

constexpr bool getParity(u32 n) {
    bool parity = false;
    for (unsigned char i = 0; i < 32; i++)
        if ((n >> i) & 1)
            parity = !parity;
    return parity;
}

constexpr bool getParity(u16 n) {
    bool parity = false;
    for (unsigned char i = 0; i < 16; i++)
        if ((n >> i) & 1)
            parity = !parity;
    return parity;
}

constexpr bool getParity(u8 n) {
    bool parity = false;
    for (unsigned char i = 0; i < 8; i++)
        if ((n >> i) & 1)
            parity = !parity;
    return parity;
}

namespace instructions {
    // execute a syscall, switching on the value in AX
    void executeSyscall(TPU& tpu, Memory& memory) {
        // switch on AX register value
        u16 syscallCode = tpu.readRegister16(Register::AX).getValue();
        switch (syscallCode) {
            case Syscall::STDOUT:
            case Syscall::STDERR: {
                u16 charPtr = tpu.readRegister16(Register::BX).getValue(); // get address for string start
                u16 length = tpu.readRegister16(Register::CX).getValue(); // get the length of string

                // load source index from BX and destination index from BX + length
                tpu.moveToRegister(Register::SI, charPtr);
                tpu.moveToRegister(Register::DI, charPtr + length);
                const u16 DI = tpu.readRegister16(Register::DI).getValue();

                while (tpu.readRegister16(Register::SI).getValue() != DI) {
                    if (syscallCode == Syscall::STDOUT) {
                        std::cout << (char)memory[tpu.readRegister16(Register::SI)++].getValue() << std::flush;
                    } else {
                        std::cerr << (char)memory[tpu.readRegister16(Register::SI)++].getValue() << std::flush;
                    }
                    
                    // sleep between writes
                    tpu.sleep();
                }
                break;
            }
            case Syscall::STDIN: {
                u16 charPtr = tpu.readRegister16(Register::BX).getValue(); // get address for string start
                u8 length = tpu.readRegister16(Register::CX).getValue(); // get the length of string

                // load source index from BX and destination index from BX + length
                tpu.moveToRegister(Register::SI, charPtr);
                tpu.moveToRegister(Register::DI, charPtr + length);
                const u16 DI = tpu.readRegister16(Register::DI).getValue();

                // init curses for Linux for waiting on character input
                #if !defined(WIN32) && !defined(_WIN32) && !defined(__WIN32) || defined(__CYGWIN__)
                    initscr();
                #endif

                // read until length is met
                while (tpu.readRegister16(Register::SI).getValue() != DI) {
                    for (u8 i = 0; i < length; i++) {
                        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
                            memory[tpu.readRegister16(Register::SI)++] = getch();
                        #else
                            memory[tpu.readRegister16(Register::SI)++] = getch();
                        #endif
                    }
                    
                    // sleep between reads
                    tpu.sleep();
                }

                #if !defined(WIN32) && !defined(_WIN32) && !defined(__WIN32) || defined(__CYGWIN__)
                    endwin();
                #endif
                break;
            }
            case Syscall::EXIT_STATUS: {
                // get exit status
                u16 exitStatus = tpu.readRegister16(Register::BX).getValue();
                tpu.setExitCode(exitStatus);
                break;
            }
            case Syscall::MALLOC: {
                // grab size from CX
                u16 size = tpu.readRegister16(Register::CX).getValue();

                // invoke malloc
                u16 addr = heapAlloc(size);
                tpu.moveToRegister(Register::DX, addr); // put address into DX
                break;
            }
            case Syscall::REALLOC: {
                // grab address from BX and size from CX
                u16 addr = tpu.readRegister16(Register::BX).getValue();
                u16 size = tpu.readRegister16(Register::CX).getValue();

                // invoke realloc
                u16 resAddr = heapRealloc(addr, size);
                tpu.moveToRegister(Register::DX, resAddr); // put address into DX
                break;
            }
            case Syscall::FREE: {
                // grab address from BX & free
                heapFree( tpu.readRegister16(Register::BX).getValue() );
                break;
            }
            default: {
                throw std::invalid_argument("Invalid syscall code: " + syscallCode);
                break;
            }
        }
    }

    void processCALL(TPU& tpu, Memory& memory) {
        // Moves the instruction pointer to a named label's entry address, storing the current instruction pointer on the callstack.

        // get jump destination address (increments IP twice)
        u16 destAddr = tpu.readWord(memory).getValue();

        // store IP on callstack
        u16 callstackAddr = tpu.readRegister16(Register::CP).getValue();
        u16 prevIP = tpu.readRegister16(Register::IP).getValue();
        memory[callstackAddr] = prevIP & 0x00FF;
        memory[callstackAddr+1] = (prevIP & 0xFF00) >> 8;

        // update callstack ptr 
        tpu.moveToRegister(Register::CP, callstackAddr + 2);

        // jump to destination address
        tpu.moveToRegister(Register::IP, destAddr);

        // sleep after storing IP
        tpu.sleep();
    }

    void processRET(TPU& tpu, Memory& memory) {
        // Revert the instruction pointer to the previous memory address stored on top of the callstack.
        u16 callstackAddr = tpu.readRegister16(Register::CP).getValue();
        u16 destAddr = memory[callstackAddr-1].getValue();
        destAddr <<= 8;
        destAddr |= memory[callstackAddr-2].getValue();

        // update callstack ptr 
        tpu.moveToRegister(Register::CP, callstackAddr - 2);

        // jump to destination address
        tpu.moveToRegister(Register::IP, destAddr);

        // sleep after storing IP
        tpu.sleep();
    }

    void processJMP(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get operands
        u16 destAddr = tpu.readWord(memory).getValue();
        switch (mod.getValue() & 0b111) {
            case 0: { // Moves the instruction pointer to the specified label.
                tpu.moveToRegister(Register::IP, destAddr);
                break;
            }
            case 1: { // Moves the instruction pointer to the specified label, if the zero flag (ZF) is set.
                if (tpu.getFlag(ZERO)) {
                    tpu.moveToRegister(Register::IP, destAddr);
                }
                break;
            }
            case 2: { // Moves the instruction pointer to the specified label, if the zero flag (ZF) is cleared.
                if (!tpu.getFlag(ZERO)) {
                    tpu.moveToRegister(Register::IP, destAddr);
                }
                break;
            }
            case 3: { // Moves the instruction pointer to the specified label, if the carry flag (CF) is set.
                if (tpu.getFlag(CARRY)) {
                    tpu.moveToRegister(Register::IP, destAddr);
                }
                break;
            }
            case 4: { // Moves the instruction pointer to the specified label, if the carry flag (CF) is cleared.
                if (!tpu.getFlag(CARRY)) {
                    tpu.moveToRegister(Register::IP, destAddr);
                }
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: JMP.");
                break;
            }
        }
    }

    void processMOV(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get operands
        switch (mod.getValue() & 0b111) {
            case 0: { // Move imm8 into address in memory.
                u16 opA = tpu.readWord(memory).getValue();
                memory[opA] = tpu.readByte(memory).getValue();
                break;
            }
            case 1: { // Move value in 8-bit register to memory address.
                u16 opA = tpu.readWord(memory).getValue();
                u8 opB = tpu.readByte(memory).getValue();
                Register reg = getRegister8FromCode(opB);
                memory[opA] = tpu.readRegister8(reg);
                break;
            }
            case 2: { // Move imm8 into 8-bit register.
                u8 opA = tpu.readByte(memory).getValue();
                tpu.moveToRegister(
                    getRegister8FromCode(opA), tpu.readByte(memory).getValue()
                );
                break;
            }
            case 3: { // Move 8-bit value from memory address into 8-bit register.
                u8 opA = tpu.readByte(memory).getValue();
                u16 addr = tpu.readWord(memory).getValue();
                tpu.moveToRegister(
                    getRegister8FromCode(opA), memory[addr].getValue()
                );
                break;
            }
            case 4: { // Move value between 8-bit registers.
                u8 opA = tpu.readByte(memory).getValue();
                u8 opB = tpu.readByte(memory).getValue();
                tpu.moveToRegister(
                    getRegister8FromCode(opA),
                    tpu.readRegister8(getRegister8FromCode(opB)).getValue()
                );
                break;
            }
            case 5: { // Move value from a memory address at an offset to a pointer register (SP, BP, CP) to an 8-bit register.
                Register refReg = getRegister16FromCode(tpu.readByte(memory).getValue());
                int offsetU16 = tpu.readWord(memory).getValue();
                int offset = (short)offsetU16;
                u16 memAddr = (int)tpu.readRegister16(refReg).getValue() + offset;
                Register regA = getRegister8FromCode(tpu.readByte(memory).getValue());
                memory[memAddr] = tpu.readRegister8( regA );
                break;
            }
            case 6: { // Move value from an 8-bit register to the memory address at an offset from a pointer register (SP, BP, CP).
                Register regB = getRegister8FromCode(tpu.readByte(memory).getValue());
                Register refReg = getRegister16FromCode(tpu.readByte(memory).getValue());
                int offsetU16 = tpu.readWord(memory).getValue();
                int offset = (short)offsetU16;
                u16 memAddr = (int)tpu.readRegister16(refReg).getValue() + offset;
                tpu.moveToRegister(regB, memory[memAddr].getValue());
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: mov.");
                break;
            }
        }
    }

    void processMOVW(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get operands
        switch (mod.getValue() & 0b111) {
            case 0: { // Move imm16 into 16-bit register.
                u8 opA = tpu.readByte(memory).getValue();
                tpu.moveToRegister(
                    getRegister16FromCode(opA), tpu.readWord(memory).getValue()
                );
                break;
            }
            case 1: { // Move value between 16-bit registers.
                u8 opA = tpu.readByte(memory).getValue();
                u8 opB = tpu.readByte(memory).getValue();
                tpu.moveToRegister(
                    getRegister16FromCode(opA),
                    tpu.readRegister16(getRegister16FromCode(opB)).getValue()
                );
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: movw.");
                break;
            }
        }
    }

    void processPUSH(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get operands
        u16 pushedValue;
        switch (mod.getValue() & 0b111) {
            case 0: { // Pushes the value of an 8-bit register onto the stack.
                pushedValue = tpu.readRegister8(getRegister8FromCode(tpu.readByte(memory).getValue())).getValue();

                // move the stack pointer up
                u16 oldAddr = tpu.readRegister16(Register::SP).getValue();
                u16 newAddr = oldAddr + 1;
                tpu.moveToRegister(Register::SP, newAddr);

                // push value onto stack at previous SP address
                memory[oldAddr] = pushedValue & 0xFF;
                break;
            }
            case 1: { // Pushes the value of a 16-bit register onto the stack, lowest byte first.
                pushedValue = tpu.readRegister16(getRegister16FromCode(tpu.readByte(memory).getValue())).getValue();

                // move the stack pointer up
                u16 lowerAddr = tpu.readRegister16(Register::SP).getValue();
                u16 upperAddr = lowerAddr + 1;
                u16 newAddr = lowerAddr + 2;
                tpu.moveToRegister(Register::SP, newAddr);

                // push value onto stack at previous SP address
                memory[lowerAddr] = pushedValue & 0x00FF;
                memory[upperAddr] = (pushedValue & 0xFF00) >> 8;
                break;
            }
            case 2: { // Pushes an imm8 value onto the stack.
                pushedValue = tpu.readByte(memory).getValue();

                // move the stack pointer up
                u16 oldAddr = tpu.readRegister16(Register::SP).getValue();
                u16 newAddr = oldAddr + 1;
                tpu.moveToRegister(Register::SP, newAddr);

                // push value onto stack at previous SP address
                memory[oldAddr] = pushedValue & 0xFF;
                break;
            }
            case 3: { // Pushes an imm16 value onto the stack, lowest byte first.
                pushedValue = tpu.readWord(memory).getValue();

                // move the stack pointer up
                u16 lowerAddr = tpu.readRegister16(Register::SP).getValue();
                u16 upperAddr = lowerAddr + 1;
                u16 newAddr = lowerAddr + 2;
                tpu.moveToRegister(Register::SP, newAddr);

                // push value onto stack at previous SP address
                memory[lowerAddr] = pushedValue & 0x00FF;
                memory[upperAddr] = (pushedValue & 0xFF00) >> 8;
                break;
            }
            case 4: { // Pushes an 8-bit value from an address in memory onto the stack.
                pushedValue = memory[ tpu.readWord(memory) ].getValue();

                // move the stack pointer up
                u16 oldAddr = tpu.readRegister16(Register::SP).getValue();
                u16 newAddr = oldAddr + 1;
                tpu.moveToRegister(Register::SP, newAddr);

                // push value onto stack at previous SP address
                memory[oldAddr] = pushedValue & 0xFF;
                break;
            }
            case 5: { // Pushes an 8-bit value from a relative address below the stack pointer onto the stack.
                // get memory address
                Register regA = getRegister16FromCode(tpu.readByte(memory).getValue());
                int offset = (int)tpu.readWord(memory).getValue();
                u16 memAddr = tpu.readRegister16( regA ).getValue() + offset;
                pushedValue = memory[memAddr].getValue();

                // move the stack pointer up
                u16 oldAddr = tpu.readRegister16(Register::SP).getValue();
                u16 newAddr = oldAddr + 1;
                tpu.moveToRegister(Register::SP, newAddr);

                // push value onto stack at previous SP address
                memory[oldAddr] = pushedValue & 0xFF;
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: push.");
                break;
            }
        }
    }

    void processPOP(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get operands
        u16 oldAddr = tpu.readRegister16(Register::SP).getValue();
        u16 newAddr = oldAddr - 1;
        u16 poppedValue = memory[ newAddr ].getValue();
        switch (mod.getValue() & 0b111) {
            case 0: { // Pops the last byte off the stack to an 8-bit register.
                tpu.moveToRegister(getRegister8FromCode(tpu.readByte(memory).getValue()), poppedValue);
                break;
            }
            case 1: break; // Pops the last byte off the stack without storing it.
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: pop.");
                break;
            }
        }

        // move the stack pointer back down
        tpu.moveToRegister(Register::SP, newAddr);
    }

    void processPOPW(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get operands
        u16 oldAddr = tpu.readRegister16(Register::SP).getValue();
        u16 upperAddr = oldAddr - 1;
        u16 lowerAddr = oldAddr - 2;
        u16 poppedValue = memory[ upperAddr ].getValue();
        poppedValue <<= 8;
        poppedValue |= memory[ lowerAddr ].getValue();

        switch (mod.getValue() & 0b111) {
            case 0: { // Pops the top two bytes off the stack to a 16-bit register, the top byte into the upper half.
                tpu.moveToRegister(getRegister16FromCode(tpu.readByte(memory).getValue()), poppedValue);
                break;
            }
            case 1: break; // Pops the top two bytes off the stack without storing them.
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: popw.");
                break;
            }
        }

        // move the stack pointer back down
        tpu.moveToRegister(Register::SP, lowerAddr);
    }

    void processADD(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get operands
        u8 opA = tpu.readByte(memory).getValue();
        Register dest;

        // switch based on signedness
        const bool isSignedOp = mod.getValue() & 8;
        switch (mod.getValue() & 0b111) {
            case 0:   // Adds 8-bit register and imm8 and stores in first operand.
            case 2: { // Adds two 8-bit registers and stores in first operand.
                dest = getRegister8FromCode(opA);
                u8 uA = tpu.readRegister8(dest).getValue();
                u8 uB;
                if ((mod.getValue() & 0b111) == 2) {
                    Register src = getRegister8FromCode(tpu.readByte(memory).getValue());
                    uB = tpu.readRegister8(src).getValue();
                } else {
                    uB = tpu.readByte(memory).getValue();
                }
                u8 sum8 = 0;
                bool isCarry = false;

                if (!isSignedOp) { // unsigned operation
                    sum8 = uA + uB;
                    isCarry = ((u16)uA + (u16)uB) > 0xFF;
                } else { // signed operation
                    // if negative, extract unsigned to signed neg
                    s8 A = (uA & 0x80) ? -(0x7F - (uA & 0x7F) + 1) : (uA & 0x7F);
                    s8 B = (uB & 0x80) ? -(0x7F - (uB & 0x7F) + 1) : (uB & 0x7F);
                    s16 ssum16 = (s16)A + (s16)B;

                    // copy bits (don't trust typecasts)
                    sum8 |= A + B;
                    isCarry = ssum16 > 0x7F || ssum16 < -0x80;
                }

                // store result & update flags
                tpu.moveToRegister( dest, sum8 );
                tpu.setFlag(CARRY, isCarry); // same as overflow
                tpu.setFlag(PARITY, getParity(sum8));
                tpu.setFlag(ZERO, sum8 == 0);
                tpu.setFlag(SIGN, sum8 & 0x80);
                tpu.setFlag(OVERFLOW, isCarry);
                break;
            }
            case 1:   // Adds 16-bit register and imm16 and stores in first operand.
            case 3: { // Adds two 16-bit registers and stores in first operand.
                dest = getRegister16FromCode(opA);
                u16 uA = tpu.readRegister16(dest).getValue();
                u16 uB;
                if ((mod.getValue() & 0b111) == 3) {
                    Register src = getRegister16FromCode(tpu.readByte(memory).getValue());
                    uB = tpu.readRegister16(src).getValue();
                } else {
                    uB = tpu.readWord(memory).getValue();
                }
                u16 sum16 = 0;
                bool isCarry = false;

                if (!isSignedOp) { // unsigned operation
                    sum16 = uA + uB;
                    isCarry = ((u32)uA + (u32)uB) > 0xFFFF;
                } else { // signed operation
                    // if negative, extract unsigned to signed neg
                    s16 A = (uA & 0x8000) ? -(0x7FFF - (uA & 0x7FFF) + 1) : (uA & 0x7FFF);
                    s16 B = (uB & 0x8000) ? -(0x7FFF - (uB & 0x7FFF) + 1) : (uB & 0x7FFF);
                    s32 ssum32 = (s32)A + (s32)B;

                    // copy bits (don't trust typecasts)
                    sum16 |= A + B;
                    isCarry = ssum32 > 0x7FFF || ssum32 < -0x8000;
                }

                // store result & update flags
                tpu.moveToRegister( dest, sum16 );
                tpu.setFlag(CARRY, isCarry); // same as overflow
                tpu.setFlag(PARITY, getParity(sum16));
                tpu.setFlag(ZERO, sum16 == 0);
                tpu.setFlag(SIGN, sum16 & 0x8000);
                tpu.setFlag(OVERFLOW, isCarry);
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: add/sadd.");
                break;
            }
        }
    }

    void processSUB(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get operands
        u8 opA = tpu.readByte(memory).getValue();
        Register dest;

        // switch based on signedness
        const bool isSignedOp = mod.getValue() & 8;
        switch (mod.getValue() & 0b111) {
            case 0:   // Subtracts imm8 from an 8-bit register and stores in first operand.
            case 2: { // Subtracts 8-bit registers (second operand from first) and stores in first operand.
                dest = getRegister8FromCode(opA);
                u8 uA = tpu.readRegister8(dest).getValue();
                u8 uB;
                if ((mod.getValue() & 0b111) == 2) {
                    Register src = getRegister8FromCode(tpu.readByte(memory).getValue());
                    uB = tpu.readRegister8(src).getValue();
                } else {
                    uB = tpu.readByte(memory).getValue();
                }
                u8 diff8 = 0;
                bool isBorrow = false;

                if (!isSignedOp) { // unsigned operation
                    diff8 = uA - uB;
                    isBorrow = uB > uA;
                } else { // signed operation
                    // if negative, extract unsigned to signed neg
                    s8 A = (uA & 0x80) ? -(0x7F - (uA & 0x7F) + 1) : (uA & 0x7F);
                    s8 B = (uB & 0x80) ? -(0x7F - (uB & 0x7F) + 1) : (uB & 0x7F);

                    // copy bits (don't trust typecasts)
                    diff8 |= A - B;
                    isBorrow = B > A;
                }

                // store result & update flags
                tpu.moveToRegister( dest, diff8 );
                tpu.setFlag(CARRY, isBorrow); // same as overflow
                tpu.setFlag(PARITY, getParity(diff8));
                tpu.setFlag(ZERO, diff8 == 0);
                tpu.setFlag(SIGN, diff8 & 0x80);
                tpu.setFlag(OVERFLOW, isBorrow);
                break;
            }
            case 1:   // Subtracts imm16 from a 16-bit register and stores in first operand.
            case 3: { // Subtracts 16-bit registers (second operand from first) and stores in first operand.
                dest = getRegister16FromCode(opA);
                u16 uA = tpu.readRegister16(dest).getValue();
                u16 uB;
                if ((mod.getValue() & 0b111) == 3) {
                    Register src = getRegister16FromCode(tpu.readByte(memory).getValue());
                    uB = tpu.readRegister16(src).getValue();
                } else {
                    uB = tpu.readWord(memory).getValue();
                }
                u16 diff16 = 0;
                bool isBorrow = false;

                if (!isSignedOp) { // unsigned operation
                    diff16 = uA - uB;
                    isBorrow = uB > uA;
                } else { // signed operation
                    // if negative, extract unsigned to signed neg
                    s16 A = (uA & 0x8000) ? -(0x7FFF - (uA & 0x7FFF) + 1) : (uA & 0x7FFF);
                    s16 B = (uB & 0x8000) ? -(0x7FFF - (uB & 0x7FFF) + 1) : (uB & 0x7FFF);

                    // copy bits (don't trust typecasts)
                    diff16 |= A - B;
                    isBorrow = B > A;
                }

                // store result & update flags
                tpu.moveToRegister( dest, diff16 );
                tpu.setFlag(CARRY, isBorrow); // same as overflow
                tpu.setFlag(PARITY, getParity(diff16));
                tpu.setFlag(ZERO, diff16 == 0);
                tpu.setFlag(SIGN, diff16 & 0x8000);
                tpu.setFlag(OVERFLOW, isBorrow);
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: sub/ssub.");
                break;
            }
        }
    }

    void processMUL(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // multiply operands
        const bool isSignedOp = mod.getValue() & 8;
        switch (mod.getValue() & 0b111) {
            case 0:   // Multiplies the AL register by imm8 and stores the product in the 16-bit AX register.
            case 2: { // Multiplies the AL register by an 8-bit register and stores the product in the 16-bit AX register.
                u8 uA = tpu.readRegister8(Register::AL).getValue();
                u8 uB;
                if ((mod.getValue() & 0b111) == 2) {
                    uB = tpu.readRegister8( getRegister8FromCode(tpu.readByte(memory).getValue()) ).getValue();
                } else {
                    uB = tpu.readByte(memory).getValue();
                }
                u16 product = 0;
                bool isCarry = false;

                if (!isSignedOp) {
                    product = uA * uB;
                    isCarry = product > 0xFF;
                } else {
                    // handle signed multiplication
                    s8 A = (uA & 0x80) ? -(0x7F - (uA & 0x7F) + 1) : (uA & 0x7F);
                    s8 B = (uB & 0x80) ? -(0x7F - (uB & 0x7F) + 1) : (uB & 0x7F);
                    s16 sproduct = (s16)A * (s16)B;
                    product |= sproduct;
                    isCarry = sproduct > 0x7F || sproduct < -0x80;
                }

                // move value & update flags
                tpu.moveToRegister(Register::AX, product);
                tpu.setFlag(CARRY, isCarry); // same as overflow
                tpu.setFlag(PARITY, getParity(product));
                tpu.setFlag(ZERO, product == 0);
                tpu.setFlag(SIGN, product & 0x8000);
                tpu.setFlag(OVERFLOW, isCarry);
                break;
            }
            case 1:   // Multiplies the AX register by imm16 and stores the lower half of the product in the 16-bit AX register and upper half in the 16-bit DX register.
            case 3: { // Multiplies the AX register by an 8-bit register and stores the lower half of the product in the 16-bit AX register and upper half in the 16-bit DX register.
                u16 uA = tpu.readRegister16(Register::AX).getValue();
                u16 uB;
                if ((mod.getValue() & 0b111) == 3) {
                    uB = tpu.readRegister16( getRegister16FromCode(tpu.readByte(memory).getValue()) ).getValue();
                } else {
                    uB = tpu.readWord(memory).getValue();
                }
                u32 product = 0;
                bool isCarry = false;

                if (!isSignedOp) {
                    product = uA * uB;
                    isCarry = product > 0xFF;
                } else {
                    // handle signed multiplication
                    s16 A = (uA & 0x8000) ? -(0x7FFF - (uA & 0x7FFF) + 1) : (uA & 0x7FFF);
                    s16 B = (uB & 0x8000) ? -(0x7FFF - (uB & 0x7FFF) + 1) : (uB & 0x7FFF);
                    s32 sproduct = (s32)A * (s32)B;
                    product |= sproduct;
                    isCarry = sproduct > 0x7FFF || sproduct < -0x8000;
                }

                // move value & update flags
                u16 lower = product & 0xFFFF;
                u16 upper = product >> 16;
                tpu.moveToRegister(Register::AX, lower);
                tpu.moveToRegister(Register::DX, upper);

                tpu.setFlag(CARRY, isCarry); // same as overflow
                tpu.setFlag(PARITY, getParity(product));
                tpu.setFlag(ZERO, product == 0);
                tpu.setFlag(SIGN, upper & 0x8000);
                tpu.setFlag(OVERFLOW, isCarry);
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: mul/smul.");
                break;
            }
        }
    }

    void processDIV(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // divide operands
        const bool isSignedOp = mod.getValue() & 8;
        switch (mod.getValue() & 0b111) {
            case 0:   // Divides the AL register by imm8 and stores the dividend in AL and remainder AH.
            case 2: { // Multiplies the AL register by an 8-bit register and stores the product in the 16-bit AX register.
                u8 uA = tpu.readRegister8(Register::AL).getValue();
                u8 uB;
                if ((mod.getValue() & 0b111) == 2) {
                    uB = tpu.readRegister8( getRegister8FromCode(tpu.readByte(memory).getValue()) ).getValue();
                } else {
                    uB = tpu.readByte(memory).getValue();
                }
                u8 dividend = 0, remainder = 0;

                if (!isSignedOp) {
                    dividend = uA / uB;
                    remainder = uA % uB;
                } else {
                    // handle signed multiplication
                    s8 A = (uA & 0x80) ? -(0x7F - (uA & 0x7F) + 1) : (uA & 0x7F);
                    s8 B = (uB & 0x80) ? -(0x7F - (uB & 0x7F) + 1) : (uB & 0x7F);
                    dividend  |= A / B;
                    remainder |= A % B;
                }

                // set value & update flags
                tpu.moveToRegister(Register::AL, dividend);
                tpu.moveToRegister(Register::AH, remainder);

                tpu.setFlag(CARRY, remainder == 0); // same as overflow
                tpu.setFlag(PARITY, getParity(dividend));
                tpu.setFlag(ZERO, dividend == 0);
                tpu.setFlag(SIGN, dividend & 0x80);
                tpu.setFlag(OVERFLOW, remainder == 0);
                break;
            }
            case 1:   // Divides the AX register by imm16 and stores the dividend in AX and remainder DX.
            case 3: { // Divides the AX register by a 16-bit register and stores the dividend in AX and remainder DX.
                u16 uA = tpu.readRegister16(Register::AX).getValue();
                u16 uB;
                if ((mod.getValue() & 0b111) == 3) {
                    uB = tpu.readRegister16( getRegister16FromCode(tpu.readByte(memory).getValue()) ).getValue();
                } else {
                    uB = tpu.readWord(memory).getValue();
                }
                u16 dividend = 0, remainder = 0;

                if (!isSignedOp) {
                    dividend = uA / uB;
                    remainder = uA % uB;
                } else {
                    // handle signed multiplication
                    s16 A = (uA & 0x8000) ? -(0x7FFF - (uA & 0x7FFF) + 1) : (uA & 0x7FFF);
                    s16 B = (uB & 0x8000) ? -(0x7FFF - (uB & 0x7FFF) + 1) : (uB & 0x7FFF);
                    dividend  |= A / B;
                    remainder |= A % B;
                }

                // set value & update flags
                tpu.moveToRegister(Register::AX, dividend);
                tpu.moveToRegister(Register::DX, remainder);

                tpu.setFlag(CARRY, remainder == 0); // same as overflow
                tpu.setFlag(PARITY, getParity(dividend));
                tpu.setFlag(ZERO, dividend == 0);
                tpu.setFlag(SIGN, dividend & 0x8000);
                tpu.setFlag(OVERFLOW, remainder == 0);
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: div.");
                break;
            }
        }
    }

    void processCMP(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get operands
        u8 opA = tpu.readByte(memory).getValue();
        Register regA;

        // switch based on signedness
        const bool isSignedOp = mod.getValue() & 8;
        switch (mod.getValue() & 0b111) {
            case 0:   // Compares an 8-bit register value and imm8.
            case 2: { // Compares two 8-bit registers.
                regA = getRegister8FromCode(opA);
                u8 uA = tpu.readRegister8(regA).getValue();
                u8 uB;
                if ((mod.getValue() & 0b111) == 2) {
                    Register regB = getRegister8FromCode(tpu.readByte(memory).getValue());
                    uB = tpu.readRegister8(regB).getValue();
                } else {
                    uB = tpu.readByte(memory).getValue();
                }
                u8 diff8 = 0;
                bool isCarry = false;

                if (!isSignedOp) { // unsigned operation
                    diff8 = uA - uB;
                    isCarry = uB > uA;
                } else { // signed operation
                    // if negative, extract unsigned to signed neg
                    s8 A = (uA & 0x80) ? -(0x7F - (uA & 0x7F) + 1) : (uA & 0x7F);
                    s8 B = (uB & 0x80) ? -(0x7F - (uB & 0x7F) + 1) : (uB & 0x7F);

                    // copy bits (don't trust typecasts)
                    diff8 |= A - B;
                    isCarry = B > A;
                }

                // update flags
                tpu.setFlag(CARRY, isCarry); // same as overflow
                tpu.setFlag(PARITY, getParity(diff8));
                tpu.setFlag(ZERO, diff8 == 0);
                tpu.setFlag(OVERFLOW, isCarry);
                break;
            }
            case 1:   // Compares a 16-bit register value and imm16.
            case 3: { // Compares two 16-bit registers.
                regA = getRegister16FromCode(opA);
                u16 uA = tpu.readRegister16(regA).getValue();
                u16 uB;
                if ((mod.getValue() & 0b111) == 3) {
                    Register regB = getRegister16FromCode(tpu.readByte(memory).getValue());
                    uB = tpu.readRegister16(regB).getValue();
                } else {
                    uB = tpu.readWord(memory).getValue();
                }
                u16 diff16 = 0;
                bool isCarry = false;

                if (!isSignedOp) { // unsigned operation
                    diff16 = uA - uB;
                    isCarry = uB > uA;
                } else { // signed operation
                    // if negative, extract unsigned to signed neg
                    s16 A = (uA & 0x8000) ? -(0x7FFF - (uA & 0x7FFF) + 1) : (uA & 0x7FFF);
                    s16 B = (uB & 0x8000) ? -(0x7FFF - (uB & 0x7FFF) + 1) : (uB & 0x7FFF);

                    // copy bits (don't trust typecasts)
                    diff16 |= A - B;
                    isCarry = B > A;
                }

                // update flags
                tpu.setFlag(CARRY, isCarry); // same as overflow
                tpu.setFlag(PARITY, getParity(diff16));
                tpu.setFlag(ZERO, diff16 == 0);
                tpu.setFlag(OVERFLOW, isCarry);
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: cmp/scmp.");
                break;
            }
        }
    }

    void processBUF(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get operands
        u16 value;
        switch (mod.getValue() & 0b111) {
            case 0: { // Buffers a value from an 8-bit register, updating the flags according to the register value.
                u8 opA = tpu.readByte(memory).getValue();
                value = tpu.readRegister8(getRegister8FromCode(opA)).getValue();
                break;
            }
            case 1: { // Buffers a value from a 16-bit register, updating the flags according to the register value.
                u8 opA = tpu.readByte(memory).getValue();
                value = tpu.readRegister16(getRegister16FromCode(opA)).getValue();
                break;
            }
            case 2: { // Buffers an imm8 value, updating the flags according to the value.
                value = tpu.readByte(memory).getValue();
                break;
            }
            case 3: { // Buffers an imm16 value, updating the flags according to the value.
                value = tpu.readWord(memory).getValue();
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: not.");
                break;
            }
        }

        // update flags
        tpu.setFlag(CARRY, 0); // same as overflow
        tpu.setFlag(PARITY, getParity(value));
        tpu.setFlag(ZERO, value == 0);
        tpu.setFlag(SIGN, (value & 128) > 0);
        tpu.setFlag(OVERFLOW, 0);
    }

    void processAND(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get operands
        u8 opA = tpu.readByte(memory).getValue();
        switch (mod.getValue() & 0b111) {
            case 0: { // Logical AND between 8-bit register and imm8, stored in first operand.
                u8 A = tpu.readRegister8(getRegister8FromCode(opA)).getValue();
                u8 result = A & tpu.readByte(memory).getValue();
                tpu.moveToRegister( getRegister8FromCode(opA), result );

                // update flags
                tpu.setFlag(PARITY, getParity(result));
                tpu.setFlag(ZERO, result == 0);
                tpu.setFlag(SIGN, (result & 128) > 0);
                break;
            }
            case 1: { // Logical AND between 16-bit register and imm16, stored in first operand.
                u16 A = tpu.readRegister16(getRegister16FromCode(opA)).getValue();
                u16 result = A & tpu.readWord(memory).getValue();
                tpu.moveToRegister( getRegister16FromCode(opA), result );

                // update flags
                tpu.setFlag(PARITY, getParity(result));
                tpu.setFlag(ZERO, result == 0);
                tpu.setFlag(SIGN, (result & (1u << 15)) > 0);
                break;
            }
            case 2: { // Logical AND between two 8-bit registers, stored in first operand.
                u8 opB = tpu.readByte(memory).getValue();
                u8 A = tpu.readRegister8(getRegister8FromCode(opA)).getValue();
                u8 B = tpu.readRegister8(getRegister8FromCode(opB)).getValue();
                u8 result = A & B;
                tpu.moveToRegister( getRegister8FromCode(opA), result );

                // update flags
                tpu.setFlag(PARITY, getParity(result));
                tpu.setFlag(ZERO, result == 0);
                tpu.setFlag(SIGN, (result & 128) > 0);
                break;
            }
            case 3: { // Logical AND between two 16-bit registers, stored in first operand.
                u8 opB = tpu.readByte(memory).getValue();
                u16 A = tpu.readRegister16(getRegister16FromCode(opA)).getValue();
                u16 B = tpu.readRegister16(getRegister16FromCode(opB)).getValue();
                u16 result = A & B;
                tpu.moveToRegister( getRegister16FromCode(opA), result );

                // update flags
                tpu.setFlag(PARITY, getParity(result));
                tpu.setFlag(ZERO, result == 0);
                tpu.setFlag(SIGN, (result & (1u << 15)) > 0);
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: and.");
                break;
            }
        }
    }

    void processOR(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get operands
        u8 opA = tpu.readByte(memory).getValue();
        switch (mod.getValue() & 0b111) {
            case 0: { // Logical OR between 8-bit register and imm8, stored in first operand.
                u8 A = tpu.readRegister8(getRegister8FromCode(opA)).getValue();
                u8 result = A | tpu.readByte(memory).getValue();
                tpu.moveToRegister( getRegister8FromCode(opA), result );

                // update flags
                tpu.setFlag(PARITY, getParity(result));
                tpu.setFlag(ZERO, result == 0);
                tpu.setFlag(SIGN, (result & 128) > 0);
                break;
            }
            case 1: { // Logical OR between 16-bit register and imm16, stored in first operand.
                u16 A = tpu.readRegister16(getRegister16FromCode(opA)).getValue();
                u16 result = A | tpu.readWord(memory).getValue();
                tpu.moveToRegister( getRegister16FromCode(opA), result );

                // update flags
                tpu.setFlag(PARITY, getParity(result));
                tpu.setFlag(ZERO, result == 0);
                tpu.setFlag(SIGN, (result & (1u << 15)) > 0);
                break;
            }
            case 2: { // Logical OR between two 8-bit registers, stored in first operand.
                u8 opB = tpu.readByte(memory).getValue();
                u8 A = tpu.readRegister8(getRegister8FromCode(opA)).getValue();
                u8 B = tpu.readRegister8(getRegister8FromCode(opB)).getValue();
                u8 result = A | B;
                tpu.moveToRegister( getRegister8FromCode(opA), result );

                // update flags
                tpu.setFlag(PARITY, getParity(result));
                tpu.setFlag(ZERO, result == 0);
                tpu.setFlag(SIGN, (result & 128) > 0);
                break;
            }
            case 3: { // Logical OR between two 16-bit registers, stored in first operand.
                u8 opB = tpu.readByte(memory).getValue();
                u16 A = tpu.readRegister16(getRegister16FromCode(opA)).getValue();
                u16 B = tpu.readRegister16(getRegister16FromCode(opB)).getValue();
                u16 result = A | B;
                tpu.moveToRegister( getRegister16FromCode(opA), result );

                // update flags
                tpu.setFlag(PARITY, getParity(result));
                tpu.setFlag(ZERO, result == 0);
                tpu.setFlag(SIGN, (result & (1u << 15)) > 0);
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: or.");
                break;
            }
        }
    }

    void processXOR(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get operands
        u8 opA = tpu.readByte(memory).getValue();
        switch (mod.getValue() & 0b111) {
            case 0: { // Logical OR between 8-bit register and imm8, stored in first operand.
                u8 A = tpu.readRegister8(getRegister8FromCode(opA)).getValue();
                u8 result = A ^ tpu.readByte(memory).getValue();
                tpu.moveToRegister( getRegister8FromCode(opA), result );

                // update flags
                tpu.setFlag(PARITY, getParity(result));
                tpu.setFlag(ZERO, result == 0);
                tpu.setFlag(SIGN, (result & 128) > 0);
                break;
            }
            case 1: { // Logical OR between 16-bit register and imm16, stored in first operand.
                u16 A = tpu.readRegister16(getRegister16FromCode(opA)).getValue();
                u16 result = A ^ tpu.readWord(memory).getValue();
                tpu.moveToRegister( getRegister16FromCode(opA), result );

                // update flags
                tpu.setFlag(PARITY, getParity(result));
                tpu.setFlag(ZERO, result == 0);
                tpu.setFlag(SIGN, (result & (1u << 15)) > 0);
                break;
            }
            case 2: { // Logical OR between two 8-bit registers, stored in first operand.
                u8 opB = tpu.readByte(memory).getValue();
                u8 A = tpu.readRegister8(getRegister8FromCode(opA)).getValue();
                u8 B = tpu.readRegister8(getRegister8FromCode(opB)).getValue();
                u8 result = A ^ B;
                tpu.moveToRegister( getRegister8FromCode(opA), result );

                // update flags
                tpu.setFlag(PARITY, getParity(result));
                tpu.setFlag(ZERO, result == 0);
                tpu.setFlag(SIGN, (result & 128) > 0);
                break;
            }
            case 3: { // Logical OR between two 16-bit registers, stored in first operand.
                u8 opB = tpu.readByte(memory).getValue();
                u16 A = tpu.readRegister16(getRegister16FromCode(opA)).getValue();
                u16 B = tpu.readRegister16(getRegister16FromCode(opB)).getValue();
                u16 result = A ^ B;
                tpu.moveToRegister( getRegister16FromCode(opA), result );

                // update flags
                tpu.setFlag(PARITY, getParity(result));
                tpu.setFlag(ZERO, result == 0);
                tpu.setFlag(SIGN, (result & (1u << 15)) > 0);
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: xor.");
                break;
            }
        }
    }

    void processNOT(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get operands
        u8 opA = tpu.readByte(memory).getValue();
        switch (mod.getValue() & 0b111) {
            case 0: { // Performs bitwise NOT on an 8-bit register and stores in that register.
                u8 A = tpu.readRegister8(getRegister8FromCode(opA)).getValue();
                tpu.moveToRegister( getRegister8FromCode(opA), ~A );
                break;
            }
            case 1: { // Performs bitwise NOT on an 16-bit register and stores in that register.
                u16 A = tpu.readRegister16(getRegister16FromCode(opA)).getValue();
                tpu.moveToRegister( getRegister16FromCode(opA), ~A );
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: not.");
                break;
            }
        }
    }

    void processSHL(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get operands
        u8 opA = tpu.readByte(memory).getValue();
        u8 numShifts = tpu.readByte(memory).getValue();
        const bool isSignedOp = mod.getValue() & 8;
        switch (mod.getValue() & 0b111) {
            case 0:   // Shifts the value in the given 8-bit register left imm8 times in place.
            case 2: { // Shifts the value in the given 8-bit register left once for the value in the 8-bit register in place.
                if ((mod.getValue() & 0b111) == 2)
                    numShifts = tpu.readRegister8(getRegister8FromCode(numShifts)).getValue();
                u8 A = tpu.readRegister8(getRegister8FromCode(opA)).getValue();
                u8 value = 0;

                if (!isSignedOp) {
                    value = A << std::min((int)numShifts, 8);
                } else { // handle signed value
                    value = (A & 0x7F) << std::min((int)numShifts, 8);
                    value |= A & 0x80; // re-add sign bit
                }
                tpu.moveToRegister( getRegister8FromCode(opA), value );
                break;
            }
            case 1:   // Shifts the value in the given 16-bit register left imm8 times in place.
            case 3: { // Shifts the value in the given 16-bit register left once for the value in the 8-bit register in place.
                if ((mod.getValue() & 0b111) == 3)
                    numShifts = tpu.readRegister16(getRegister16FromCode(numShifts)).getValue();
                u16 A = tpu.readRegister16(getRegister16FromCode(opA)).getValue();
                u16 value = 0;

                if (!isSignedOp) {
                    value = A << std::min((int)numShifts, 16);
                } else { // handle signed value
                    value = (A & 0x7FFF) << std::min((int)numShifts, 16);
                    value |= A & 0x8000; // re-add sign bit
                }
                tpu.moveToRegister( getRegister16FromCode(opA), value );
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: shl/sshl.");
                break;
            }
        }
    }

    void processSHR(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get operands
        u8 opA = tpu.readByte(memory).getValue();
        u8 numShifts = tpu.readByte(memory).getValue();
        const bool isSignedOp = mod.getValue() & 8;
        switch (mod.getValue() & 0b111) {
            case 0:   // Shifts the value in the given 8-bit register right imm8 times in place.
            case 2: { // Shifts the value in the given 8-bit register right once for the value in the 8-bit register in place.
                if ((mod.getValue() & 0b111) == 2)
                    numShifts = tpu.readRegister8(getRegister8FromCode(numShifts)).getValue();
                u8 A = tpu.readRegister8(getRegister8FromCode(opA)).getValue();
                u8 value = 0;

                if (!isSignedOp) {
                    value = A >> std::min((int)numShifts, 8);
                } else { // handle signed value
                    value = (A & 0x7F) >> std::min((int)numShifts, 8);
                    value |= A & 0x80; // re-add sign bit
                }
                tpu.moveToRegister( getRegister8FromCode(opA), value );
                break;
            }
            case 1:   // Shifts the value in the given 16-bit register right imm8 times in place.
            case 3: { // Shifts the value in the given 16-bit register right once for the value in the 8-bit register in place.
                if ((mod.getValue() & 0b111) == 3)
                    numShifts = tpu.readRegister16(getRegister16FromCode(numShifts)).getValue();
                u16 A = tpu.readRegister16(getRegister16FromCode(opA)).getValue();
                u16 value = 0;

                if (!isSignedOp) {
                    value = A >> std::min((int)numShifts, 16);
                } else { // handle signed value
                    value = (A & 0x7FFF) >> std::min((int)numShifts, 16);
                    value |= A & 0x8000; // re-add sign bit
                }
                tpu.moveToRegister( getRegister16FromCode(opA), value );
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: shr/sshr.");
                break;
            }
        }
    }
}