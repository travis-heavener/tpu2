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
        switch (mod.getValue() & 0b111) {
            case 0: { // Adds 8-bit register and imm8 and stores in first operand.
                dest = getRegister8FromCode(opA);
                u8 A = tpu.readRegister8(dest).getValue();
                u8 B = tpu.readByte(memory).getValue();
                u8 sum = A + B;
                tpu.moveToRegister( dest, sum );

                // update flags
                tpu.setFlag(CARRY, ((u16)A + (u16)B) > 0xFF); // same as overflow
                tpu.setFlag(PARITY, getParity(sum));
                tpu.setFlag(ZERO, sum == 0);
                tpu.setFlag(SIGN, (sum & (1u << 7)) > 0);
                tpu.setFlag(OVERFLOW, ((u16)A + (u16)B) > 0xFF);
                break;
            }
            case 1: { // Adds 16-bit register and imm16 and stores in first operand.
                dest = getRegister16FromCode(opA);
                u16 A = tpu.readRegister16(dest).getValue();
                u16 B = tpu.readWord(memory).getValue();
                u16 sum = A + B;
                tpu.moveToRegister( dest, sum );

                // update flags
                tpu.setFlag(CARRY, ((u32)A + (u32)B) > 0xFFFF); // same as overflow
                tpu.setFlag(PARITY, getParity(sum));
                tpu.setFlag(ZERO, sum == 0);
                tpu.setFlag(SIGN, (sum & (1u << 15)) > 0);
                tpu.setFlag(OVERFLOW, ((u32)A + (u32)B) > 0xFFFF);
                break;
            }
            case 2: { // Adds two 8-bit registers and stores in first operand.
                dest = getRegister8FromCode(opA);
                Register src = getRegister8FromCode(tpu.readByte(memory).getValue());
                u8 A = tpu.readRegister8(dest).getValue();
                u8 B = tpu.readRegister8(src).getValue();
                u8 sum = A + B;
                tpu.moveToRegister( dest, sum );

                // update flags
                tpu.setFlag(CARRY, ((u16)A + (u16)B) > 0xFF); // same as overflow
                tpu.setFlag(PARITY, getParity(sum));
                tpu.setFlag(ZERO, sum == 0);
                tpu.setFlag(SIGN, (sum & (1u << 7)) > 0);
                tpu.setFlag(OVERFLOW, ((u16)A + (u16)B) > 0xFF);
                break;
            }
            case 3: { // Adds two 16-bit registers and stores in first operand.
                dest = getRegister16FromCode(opA);
                Register src = getRegister16FromCode(tpu.readByte(memory).getValue());
                u16 A = tpu.readRegister16(dest).getValue();
                u16 B = tpu.readRegister16(src).getValue();
                u16 sum = A + B;
                tpu.moveToRegister( dest, sum );

                // update flags
                tpu.setFlag(CARRY, ((u32)A + (u32)B) > 0xFFFF); // same as overflow
                tpu.setFlag(PARITY, getParity(sum));
                tpu.setFlag(ZERO, sum == 0);
                tpu.setFlag(SIGN, (sum & (1u << 15)) > 0);
                tpu.setFlag(OVERFLOW, ((u32)A + (u32)B) > 0xFFFF);
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: add.");
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
        switch (mod.getValue() & 0b111) {
            case 0: { // Subtracts imm8 from an 8-bit register and stores in first operand.
                dest = getRegister8FromCode(opA);
                u8 A = tpu.readRegister8(dest).getValue();
                u8 B = tpu.readByte(memory).getValue();
                u8 diff = A - B;
                tpu.moveToRegister( dest, diff );

                // update flags
                tpu.setFlag(CARRY, ((short)A - (short)B) < 0); // same as overflow
                tpu.setFlag(PARITY, getParity(diff));
                tpu.setFlag(ZERO, diff == 0);
                tpu.setFlag(SIGN, (diff & (1u << 7)) > 0);
                tpu.setFlag(OVERFLOW, ((short)A - (short)B) < 0);
                break;
            }
            case 1: { // Subtracts imm16 from a 16-bit register and stores in first operand.
                dest = getRegister16FromCode(opA);
                u16 A = tpu.readRegister16(dest).getValue();
                u16 B = tpu.readWord(memory).getValue();
                u16 diff = A - B;
                tpu.moveToRegister( dest, diff );

                // update flags
                tpu.setFlag(CARRY, ((int)A - (int)B) < 0); // same as overflow
                tpu.setFlag(PARITY, getParity(diff));
                tpu.setFlag(ZERO, diff == 0);
                tpu.setFlag(SIGN, (diff & (1u << 15)) > 0);
                tpu.setFlag(OVERFLOW, ((int)A - (int)B) < 0);
                break;
            }
            case 2: { // Subtracts 8-bit registers (second operand from first) and stores in first operand.
                dest = getRegister8FromCode(opA);
                Register src = getRegister8FromCode(tpu.readByte(memory).getValue());
                u8 A = tpu.readRegister8(dest).getValue();
                u8 B = tpu.readRegister8(src).getValue();
                u8 diff = A - B;
                tpu.moveToRegister( dest, diff );

                // update flags
                tpu.setFlag(CARRY, ((short)A - (short)B) < 0); // same as overflow
                tpu.setFlag(PARITY, getParity(diff));
                tpu.setFlag(ZERO, diff == 0);
                tpu.setFlag(SIGN, (diff & (1u << 7)) > 0);
                tpu.setFlag(OVERFLOW, ((short)A - (short)B) < 0);
                break;
            }
            case 3: { // Adds two 16-bit registers and stores in first operand.
                dest = getRegister16FromCode(opA);
                Register src = getRegister16FromCode(tpu.readByte(memory).getValue());
                u16 A = tpu.readRegister16(dest).getValue();
                u16 B = tpu.readRegister16(src).getValue();
                u16 diff = A - B;
                tpu.moveToRegister( dest, diff );

                // update flags
                tpu.setFlag(CARRY, ((int)A - (int)B) < 0); // same as overflow
                tpu.setFlag(PARITY, getParity(diff));
                tpu.setFlag(ZERO, diff == 0);
                tpu.setFlag(SIGN, (diff & (1u << 15)) > 0);
                tpu.setFlag(OVERFLOW, ((int)A - (int)B) < 0);
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: sub.");
                break;
            }
        }
    }

    void processMUL(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // multiply operands
        switch (mod.getValue() & 0b111) {
            case 0: { // Multiplies the AL register by imm8 and stores the product in the 16-bit AX register.
                u8 A = tpu.readRegister8(Register::AL).getValue();
                u8 B = tpu.readByte(memory).getValue();
                u16 product = A * B;
                tpu.moveToRegister(Register::AX, product);

                // update flags
                tpu.setFlag(CARRY, product > 0xFF); // same as overflow
                tpu.setFlag(PARITY, getParity(product));
                tpu.setFlag(ZERO, product == 0);
                tpu.setFlag(SIGN, (product & (1u << 15)) > 0);
                tpu.setFlag(OVERFLOW, product > 0xFF);
                break;
            }
            case 1: { // Multiplies the AX register by imm16 and stores the lower half of the product in the 16-bit AX register and upper half in the 16-bit DX register.
                u16 A = tpu.readRegister8(Register::AX).getValue();
                u16 B = tpu.readWord(memory).getValue();
                u32 product = A * B;
                u16 lower = product & 0x0000FFFF;
                u16 upper = (product & 0xFFFF0000) >> 16;

                tpu.moveToRegister(Register::AX, lower);
                tpu.moveToRegister(Register::DX, upper);

                // update flags
                tpu.setFlag(CARRY, upper > 0); // same as overflow
                tpu.setFlag(PARITY, getParity(product));
                tpu.setFlag(ZERO, product == 0);
                tpu.setFlag(SIGN, (upper & (1u << 15)) > 0);
                tpu.setFlag(OVERFLOW, upper > 0);
                break;
            }
            case 2: { // Multiplies the AL register by an 8-bit register and stores the product in the 16-bit AX register.
                u8 A = tpu.readRegister8(Register::AL).getValue();
                u8 B = tpu.readRegister8( getRegister8FromCode(tpu.readByte(memory).getValue()) ).getValue();
                u16 product = A * B;
                tpu.moveToRegister(Register::AX, product);

                // update flags
                tpu.setFlag(CARRY, product > 0xFF); // same as overflow
                tpu.setFlag(PARITY, getParity(product));
                tpu.setFlag(ZERO, product == 0);
                tpu.setFlag(SIGN, (product & (1u << 15)) > 0);
                tpu.setFlag(OVERFLOW, product > 0xFF);
                break;
            }
            case 3: { // Multiplies the AX register by an 8-bit register and stores the lower half of the product in the 16-bit AX register and upper half in the 16-bit DX register.
                u16 A = tpu.readRegister16(Register::AX).getValue();
                u16 B = tpu.readRegister16( getRegister16FromCode(tpu.readByte(memory).getValue()) ).getValue();
                u32 product = A * B;
                u16 lower = product & 0x0000FFFF;
                u16 upper = (product & 0xFFFF0000) >> 16;

                tpu.moveToRegister(Register::AX, lower);
                tpu.moveToRegister(Register::DX, upper);

                // update flags
                tpu.setFlag(CARRY, upper > 0); // same as overflow
                tpu.setFlag(PARITY, getParity(product));
                tpu.setFlag(ZERO, product == 0);
                tpu.setFlag(SIGN, (upper & (1u << 15)) > 0);
                tpu.setFlag(OVERFLOW, upper > 0);
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: mul.");
                break;
            }
        }
    }

    void processDIV(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // divide operands
        switch (mod.getValue() & 0b111) {
            case 0: { // Divides the AL register by imm8 and stores the dividend in AL and remainder AH.
                u8 A = tpu.readRegister8(Register::AL).getValue();
                u8 B = tpu.readByte(memory).getValue();
                u8 dividend = A / B, remainder = A % B;
                tpu.moveToRegister(Register::AL, dividend);
                tpu.moveToRegister(Register::AH, remainder);

                // update flags
                tpu.setFlag(CARRY, remainder == 0); // same as overflow
                tpu.setFlag(PARITY, getParity(dividend));
                tpu.setFlag(ZERO, dividend == 0);
                tpu.setFlag(SIGN, (dividend & (1u << 7)) > 0);
                tpu.setFlag(OVERFLOW, remainder == 0);
                break;
            }
            case 1: { // Multiplies the AX register by imm16 and stores the lower half of the product in the 16-bit AX register and upper half in the 16-bit DX register.
                u16 A = tpu.readRegister16(Register::AX).getValue();
                u16 B = tpu.readWord(memory).getValue();
                u16 dividend = A / B, remainder = A % B;
                tpu.moveToRegister(Register::AX, dividend);
                tpu.moveToRegister(Register::DX, remainder);

                // update flags
                tpu.setFlag(CARRY, remainder == 0); // same as overflow
                tpu.setFlag(PARITY, getParity(dividend));
                tpu.setFlag(ZERO, dividend == 0);
                tpu.setFlag(SIGN, (dividend & (1u << 15)) > 0);
                tpu.setFlag(OVERFLOW, remainder == 0);
                break;
            }
            case 2: { // Multiplies the AL register by an 8-bit register and stores the product in the 16-bit AX register.
                u8 A = tpu.readRegister8(Register::AL).getValue();
                u8 B = tpu.readRegister8( getRegister8FromCode(tpu.readByte(memory).getValue()) ).getValue();
                u8 dividend = A / B, remainder = A % B;
                tpu.moveToRegister(Register::AL, dividend);
                tpu.moveToRegister(Register::AH, remainder);

                // update flags
                tpu.setFlag(CARRY, remainder == 0); // same as overflow
                tpu.setFlag(PARITY, getParity(dividend));
                tpu.setFlag(ZERO, dividend == 0);
                tpu.setFlag(SIGN, (dividend & (1u << 7)) > 0);
                tpu.setFlag(OVERFLOW, remainder == 0);
                break;
            }
            case 3: { // Multiplies the AX register by an 8-bit register and stores the lower half of the product in the 16-bit AX register and upper half in the 16-bit DX register.
                u16 A = tpu.readRegister16(Register::AX).getValue();
                u16 B = tpu.readRegister16( getRegister16FromCode(tpu.readByte(memory).getValue()) ).getValue();
                u16 dividend = A / B, remainder = A % B;
                tpu.moveToRegister(Register::AX, dividend);
                tpu.moveToRegister(Register::DX, remainder);

                // update flags
                tpu.setFlag(CARRY, remainder == 0); // same as overflow
                tpu.setFlag(PARITY, getParity(dividend));
                tpu.setFlag(ZERO, dividend == 0);
                tpu.setFlag(SIGN, (dividend & (1u << 15)) > 0);
                tpu.setFlag(OVERFLOW, remainder == 0);
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: div.");
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
        tpu.setFlag(SIGN, (value & (1u << 7)) > 0);
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
                tpu.setFlag(SIGN, (result & (1u << 7)) > 0);
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
                tpu.setFlag(SIGN, (result & (1u << 7)) > 0);
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
                tpu.setFlag(SIGN, (result & (1u << 7)) > 0);
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
                tpu.setFlag(SIGN, (result & (1u << 7)) > 0);
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
                tpu.setFlag(SIGN, (result & (1u << 7)) > 0);
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
                tpu.setFlag(SIGN, (result & (1u << 7)) > 0);
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
        switch (mod.getValue() & 0b111) {
            case 0: { // Shifts the value in the given 8-bit register left imm8 times in place.
                u8 A = tpu.readRegister8(getRegister8FromCode(opA)).getValue();
                tpu.moveToRegister( getRegister8FromCode(opA), A << std::min((int)numShifts, 8) );
                break;
            }
            case 1: { // Shifts the value in the given 16-bit register left imm8 times in place.
                u16 A = tpu.readRegister16(getRegister16FromCode(opA)).getValue();
                tpu.moveToRegister( getRegister16FromCode(opA), A << std::min((int)numShifts, 16) );
                break;
            }
            case 2: { // Shifts the value in the given 8-bit register left once for the value in the 8-bit register in place.
                u8 A = tpu.readRegister8(getRegister8FromCode(opA)).getValue();
                numShifts = tpu.readRegister8(getRegister8FromCode(numShifts)).getValue();
                tpu.moveToRegister( getRegister8FromCode(opA), A << std::min((int)numShifts, 8) );
                break;
            }
            case 3: { // Shifts the value in the given 16-bit register left once for the value in the 8-bit register in place.
                u16 A = tpu.readRegister16(getRegister16FromCode(opA)).getValue();
                numShifts = tpu.readRegister8(getRegister8FromCode(numShifts)).getValue();
                tpu.moveToRegister( getRegister16FromCode(opA), A << std::min((int)numShifts, 16) );
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: shl.");
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
        switch (mod.getValue() & 0b111) {
            case 0: { // Shifts the value in the given 8-bit register right imm8 times in place.
                u8 A = tpu.readRegister8(getRegister8FromCode(opA)).getValue();
                tpu.moveToRegister( getRegister8FromCode(opA), A >> std::min((int)numShifts, 8) );
                break;
            }
            case 1: { // Shifts the value in the given 16-bit register right imm8 times in place.
                u16 A = tpu.readRegister16(getRegister16FromCode(opA)).getValue();
                tpu.moveToRegister( getRegister16FromCode(opA), A >> std::min((int)numShifts, 16) );
                break;
            }
            case 2: { // Shifts the value in the given 8-bit register right once for the value in the 8-bit register in place.
                u8 A = tpu.readRegister8(getRegister8FromCode(opA)).getValue();
                numShifts = tpu.readRegister8(getRegister8FromCode(numShifts)).getValue();
                tpu.moveToRegister( getRegister8FromCode(opA), A >> std::min((int)numShifts, 8) );
                break;
            }
            case 3: { // Shifts the value in the given 16-bit register right once for the value in the 8-bit register in place.
                u16 A = tpu.readRegister16(getRegister16FromCode(opA)).getValue();
                numShifts = tpu.readRegister8(getRegister8FromCode(numShifts)).getValue();
                tpu.moveToRegister( getRegister16FromCode(opA), A >> std::min((int)numShifts, 16) );
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: shr.");
                break;
            }
        }
    }
}