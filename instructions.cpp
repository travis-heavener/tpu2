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

u16 getAddress(TPU& tpu, Memory& memory) {
    // read the 16-bit offset & 8-bit op code for register
    s16 offset = tpu.readWord(memory).getValue();
    Register regCode = getRegister16FromCode( tpu.readByte(memory).getValue() );
    return (s32)tpu.readRegister16(regCode).getValue() + offset;
}

Register getReg8(TPU& tpu, Memory& memory) {
    return getRegister8FromCode(tpu.readByte(memory).getValue());
}

Register getReg16(TPU& tpu, Memory& memory) {
    return getRegister16FromCode(tpu.readByte(memory).getValue());
}

u8 readReg8(TPU& tpu, Memory& memory) {
    return tpu.readRegister8( getReg8(tpu, memory) ).getValue();
}

u16 readReg16(TPU& tpu, Memory& memory) {
    return tpu.readRegister16( getReg16(tpu, memory) ).getValue();
}

constexpr s8 getSigned8(u8 A) { // if negative, extract unsigned to signed neg
    return (A & 0x80) ? -(0x7F - (A & 0x7F) + 1) : (A & 0x7F);
}

constexpr s16 getSigned16(u16 A) { // if negative, extract unsigned to signed neg
    return (A & 0x8000) ? -(0x7FFF - (A & 0x7FFF) + 1) : (A & 0x7FFF);
}

void setFlags(TPU& tpu, u8 carry, u8 parity, u8 zero, u8 sign, u8 overflow) {
    tpu.setFlag(CARRY,    carry);
    tpu.setFlag(PARITY,   parity);
    tpu.setFlag(ZERO,     zero);
    tpu.setFlag(SIGN,     sign);
    tpu.setFlag(OVERFLOW, overflow);
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

                // read until length is met or newline is met
                bool hasReadNewline = false;
                while (!hasReadNewline && tpu.readRegister16(Register::SI).getValue() != DI) {
                    for (u8 i = 0; i < length && !hasReadNewline; i++) {
                        char lastChar = getch();
                        if (lastChar == '\n') hasReadNewline = true;
                        memory[tpu.readRegister16(Register::SI)++] = lastChar;
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
            case Syscall::ADDR_MODE: {
                // determine addressing mode from BX
                const u8 addrMode = tpu.readRegister8(Register::BL).getValue();
                if (addrMode != ADDRESS_MODE_ABSOLUTE && addrMode != ADDRESS_MODE_RELATIVE) {
                    throw std::invalid_argument("Invalid addressing mode: " + std::to_string(addrMode));
                }
                tpu.setAddressingMode(addrMode);
                break;
            }
            default: throw std::invalid_argument("Invalid syscall code."); break;
        }
    }

    void processJMP(TPU& tpu, Memory& memory, u8 opCode) {
        // get operands
        u16 destAddr = tpu.readWord(memory).getValue();
        if (tpu.getAddressingMode() == ADDRESS_MODE_RELATIVE)
            destAddr += tpu.getProgramStartIndex(memory);

        bool isPermitted;
        switch (opCode) {
            case OPCode::JMP: isPermitted = true; break;
            case OPCode::JZ:  isPermitted = tpu.getFlag(ZERO); break;
            case OPCode::JNZ: isPermitted = !tpu.getFlag(ZERO); break;
            case OPCode::JC:  isPermitted = tpu.getFlag(CARRY); break;
            case OPCode::JNC: isPermitted = !tpu.getFlag(CARRY); break;
            default: throw std::invalid_argument("Invalid MOD byte for operation: JMP/JZ/JNZ/JC/JNC.");
        }

        // jump if needed
        if (isPermitted) tpu.moveToRegister(Register::IP, destAddr);
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

        tpu.moveToRegister(Register::CP, callstackAddr + 2); // update callstack ptr 
        tpu.moveToRegister(Register::IP, destAddr); // jump to destination address
        tpu.sleep(); // sleep after storing IP
    }

    void processRET(TPU& tpu, Memory& memory) {
        // Revert the instruction pointer to the previous memory address stored on top of the callstack.
        u16 callstackAddr = tpu.readRegister16(Register::CP).getValue();
        u16 destAddr = ((u16)memory[callstackAddr-1].getValue()) << 8;
        destAddr |= memory[callstackAddr-2].getValue();

        tpu.moveToRegister(Register::CP, callstackAddr - 2); // update callstack ptr
        tpu.moveToRegister(Register::IP, destAddr); // jump to destination address
        tpu.sleep(); // sleep after storing IP
    }

    void processMOV(TPU& tpu, Memory& memory, u8 opCode) {
        // get operands
        switch (opCode) {
            case OPCode::MOVI: { // reg8, imm8
                Register regA = getReg8(tpu, memory);
                tpu.moveToRegister(regA, tpu.readByte(memory).getValue());
                break;
            }
            case OPCode::MOVWI: { // reg16, imm16
                Register regA = getReg16(tpu, memory);
                tpu.moveToRegister(regA, tpu.readWord(memory).getValue());
                break;
            }
            case OPCode::MOV: { // reg8, reg8
                Register regA = getReg8(tpu, memory);
                tpu.moveToRegister(regA, readReg8(tpu, memory));
                break;
            }
            case OPCode::MOVW: { // reg16, reg16
                Register regA = getReg16(tpu, memory);
                tpu.moveToRegister(regA, readReg16(tpu, memory));
                break;
            }
            default: throw std::invalid_argument("Invalid MOD byte for operation: mov.");
        }
    }

    void processLB(TPU& tpu, Memory& memory) {
        Register regA = getReg8(tpu, memory);
        u16 addr = getAddress(tpu, memory);
        tpu.moveToRegister(regA, memory[addr].getValue());
    }

    void processLW(TPU& tpu, Memory& memory) {
        Register regA = getReg16(tpu, memory);
        u16 addr = getAddress(tpu, memory);
        u16 value = memory[addr].getValue();
        value |= ((u16)memory[addr+1].getValue()) << 8;
        tpu.moveToRegister(regA, value);
    }

    void processSB(TPU& tpu, Memory& memory) {
        u8 value = readReg8(tpu, memory);
        u16 addr = getAddress(tpu, memory);
        memory[addr] = value;
    }

    void processSW(TPU& tpu, Memory& memory) {
        u16 value = readReg16(tpu, memory);
        u16 addr = getAddress(tpu, memory);
        memory[addr] = value & 0xFF;
        memory[addr+1] = (value >> 8) & 0xFF;
    }

    void processPUSH(TPU& tpu, Memory& memory, u8 opCode) {
        // get operands
        u16 oldAddr = tpu.readRegister16(Register::SP).getValue();
        u8 writeSize = 1;
        switch (opCode) {
            case OPCode::PUSH: { // reg8
                memory[oldAddr] = readReg8(tpu, memory);
                break;
            }
            case OPCode::PUSHW: { // reg16
                u16 value = readReg16(tpu, memory);
                memory[oldAddr] = value & 0xFF;
                memory[oldAddr+1] = (value >> 8) & 0xFF;
                ++writeSize;
                break;
            }
            case OPCode::PUSHI: { // imm8
                memory[oldAddr] = tpu.readByte(memory);
                break;
            }
            case OPCode::PUSHWI: { // imm16
                u16 value = tpu.readWord(memory).getValue();
                memory[oldAddr]   = value & 0xFF;
                memory[oldAddr+1] = (value >> 8) & 0xFF;
                ++writeSize;
                break;
            }
            case OPCode::PUSHA: { // addr
                u16 addr = getAddress(tpu, memory);
                memory[oldAddr] = memory[addr];
                break;
            }
            case OPCode::PUSHWA: { // addr (pushw)
                u16 addr = getAddress(tpu, memory);
                memory[oldAddr] = memory[addr];
                memory[oldAddr+1] = memory[addr+1];
                ++writeSize;
                break;
            }
            default: throw std::invalid_argument("Invalid MOD byte for operation: push/pushw.");
        }

        // move stack pointer up
        tpu.moveToRegister(Register::SP, oldAddr + writeSize);
    }

    void processPOP(TPU& tpu, Memory& memory, u8 opCode) {
        // get operands
        u16 oldAddr = tpu.readRegister16(Register::SP).getValue();
        u8 writeSize = 1;
        switch (opCode) {
            case OPCode::POP: { // reg8
                Register regA = getReg8(tpu, memory);
                u8 value = memory[oldAddr-1].getValue();
                tpu.moveToRegister(regA, value);
                break;
            }
            case OPCode::POPW: { // reg16
                Register regA = getReg16(tpu, memory);
                u16 value = memory[oldAddr-2].getValue();
                value |= ((u16)memory[oldAddr-1].getValue()) << 8;
                tpu.moveToRegister(regA, value);
                ++writeSize;
                break;
            }
            default: throw std::invalid_argument("Invalid MOD byte for operation: pop/popw.");
        }

        // move stack pointer up
        tpu.moveToRegister(Register::SP, oldAddr - writeSize);
    }

    void processADD(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get MOD byte
        const u8 argsFormat = mod.getValue() & 7;
        const bool isSignedOp = mod.getValue() & 8;

        // get first operand (even MOD-bytes are reg8s first)
        Register regA = (argsFormat & 1) ? getReg16(tpu, memory) : getReg8(tpu, memory);

        // get operands
        switch (argsFormat) {
            case 0:   // reg8, imm8
            case 2: { // reg8, reg8
                u8 uA = tpu.readRegister8(regA).getValue();
                u8 uB = (argsFormat == 2) ? readReg8(tpu, memory) : tpu.readByte(memory).getValue();
                u8 sum8 = uA + uB;
                bool isCarry = ((u16)uA + (u16)uB) > 0xFF;

                if (isSignedOp) { // signed operation
                    s8 A = getSigned8(uA), B = getSigned8(uB);
                    s16 ssum16 = (s16)A + (s16)B;
                    sum8 = 0;
                    sum8 |= A + B; // copy bits (don't trust typecasts)
                    isCarry = ssum16 > 0x7F || ssum16 < -0x80;
                }

                // store result & update flags
                tpu.moveToRegister( regA, sum8 );
                setFlags(tpu, isCarry, getParity(sum8), sum8 == 0, sum8 & 0x80, isCarry);
                break;
            }
            case 1:   // reg16, imm16
            case 3: { // reg16, reg16
                u16 uA = tpu.readRegister16(regA).getValue();
                u16 uB = (argsFormat == 3) ? readReg16(tpu, memory) : tpu.readWord(memory).getValue();
                u16 sum16 = uA + uB;
                bool isCarry = ((u32)uA + (u32)uB) > 0xFFFF;

                if (isSignedOp) { // signed operation
                    s16 A = getSigned16(uA), B = getSigned16(uB);
                    s32 ssum32 = (s32)A + (s32)B;
                    sum16 = 0;
                    sum16 |= A + B; // copy bits (don't trust typecasts)
                    isCarry = ssum32 > 0x7FFF || ssum32 < -0x8000;
                }

                // store result & update flags
                tpu.moveToRegister( regA, sum16 );
                setFlags(tpu, isCarry, getParity(sum16), sum16 == 0, sum16 & 0x8000, isCarry);
                break;
            }
            default: throw std::invalid_argument("Invalid MOD byte for operation: add/sadd.");
        }
    }

    void processSUB(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get MOD byte
        const u8 argsFormat = mod.getValue() & 7;
        const bool isSignedOp = mod.getValue() & 8;

        // get first operand (even MOD-bytes are reg8s first)
        Register regA = (argsFormat & 1) ? getReg16(tpu, memory) : getReg8(tpu, memory);

        // get operands
        switch (argsFormat) {
            case 0:   // reg8, imm8
            case 2: { // reg8, reg8
                u8 uA = tpu.readRegister8(regA).getValue();
                u8 uB = (argsFormat == 2) ? readReg8(tpu, memory) : tpu.readByte(memory).getValue();
                u8 diff8 = uA - uB;
                bool isBorrow = uB > uA;

                if (isSignedOp) { // signed operation
                    s8 A = getSigned8(uA), B = getSigned8(uB);
                    diff8 = 0;
                    diff8 |= A - B; // copy bits (don't trust typecasts)
                    isBorrow = B > A;
                }

                // store result & update flags
                tpu.moveToRegister( regA, diff8 );
                setFlags(tpu, isBorrow, getParity(diff8), diff8 == 0, diff8 & 0x80, isBorrow);
                break;
            }
            case 1:   // reg16, imm16
            case 3: { // reg16, reg16
                u16 uA = tpu.readRegister16(regA).getValue();
                u16 uB = (argsFormat == 3) ? readReg16(tpu, memory) : tpu.readWord(memory).getValue();
                u16 diff16 = uA - uB;
                bool isBorrow = uB > uA;

                if (isSignedOp) { // signed operation
                    s16 A = getSigned16(uA), B = getSigned16(uB);
                    diff16 = 0;
                    diff16 |= A - B; // copy bits (don't trust typecasts)
                    isBorrow = B > A;
                }

                // store result & update flags
                tpu.moveToRegister( regA, diff16 );
                setFlags(tpu, isBorrow, getParity(diff16), diff16 == 0, diff16 & 0x8000, isBorrow);
                break;
            }
            default: throw std::invalid_argument("Invalid MOD byte for operation: sub/ssub.");
        }
    }

    void processMUL(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get MOD byte
        const u8 argsFormat = mod.getValue() & 7;
        const bool isSignedOp = mod.getValue() & 8;
        switch (argsFormat) {
            case 0: case 2: { // imm8, reg8
                u8 uA = tpu.readRegister8(Register::AL).getValue();
                u8 uB = (argsFormat == 0) ? tpu.readByte(memory).getValue() : readReg8(tpu, memory);
                u16 product = uA * uB;
                bool isCarry = product > 0xFF;

                if (isSignedOp) { // signed operation
                    s8 A = getSigned8(uA), B = getSigned8(uB);
                    s16 sproduct = (s16)A * (s16)B;
                    product = 0;
                    product |= sproduct;
                    isCarry = sproduct > 0x7F || sproduct < -0x80;
                }

                // move value & update flags
                tpu.moveToRegister(Register::AX, product);
                setFlags(tpu, isCarry, getParity(product), product == 0, product & 0x8000, isCarry);
                break;
            }
            case 1: case 3: { // imm16 & reg16
                u16 uA = tpu.readRegister16(Register::AX).getValue();
                u16 uB = (argsFormat == 1) ? tpu.readWord(memory).getValue() : readReg16(tpu, memory);
                u32 product = uA * uB;
                bool isCarry = product > 0xFFFF;

                if (isSignedOp) { // signed operation
                    s16 A = getSigned16(uA), B = getSigned16(uB);
                    s32 sproduct = (s32)A * (s32)B;
                    product = 0;
                    product |= sproduct;
                    isCarry = sproduct > 0x7FFF || sproduct < -0x8000;
                }

                // move value & update flags
                tpu.moveToRegister(Register::AX, product);
                tpu.moveToRegister(Register::DX, product >> 16);
                setFlags(tpu, isCarry, getParity(product), product == 0, product & 0x8000'0000, isCarry);
                break;
            }
            default: throw std::invalid_argument("Invalid MOD byte for operation: mul/smul.");
        }
    }

    void processDIV(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get MOD byte
        const u8 argsFormat = mod.getValue() & 7;
        const bool isSignedOp = mod.getValue() & 8;
        switch (argsFormat) {
            case 0: case 2: { // imm8, reg8
                u8 uA = tpu.readRegister8(Register::AL).getValue();
                u8 uB = (argsFormat == 0) ? tpu.readByte(memory).getValue() : readReg8(tpu, memory);
                u8 dividend = uA / uB;
                u8 remainder = uA % uB;

                if (isSignedOp) { // signed operation
                    s8 A = getSigned8(uA), B = getSigned8(uB);
                    dividend = remainder = 0;
                    dividend  |= A / B;
                    remainder |= A % B;
                }

                // move value & update flags
                bool isCarry = remainder == 0;
                tpu.moveToRegister(Register::AL, dividend);
                tpu.moveToRegister(Register::AH, remainder);
                setFlags(tpu, isCarry, getParity(dividend), dividend == 0, dividend & 0x80, isCarry);
                break;
            }
            case 1: case 3: { // imm16 & reg16
                u16 uA = tpu.readRegister16(Register::AX).getValue();
                u16 uB = (argsFormat == 1) ? tpu.readWord(memory).getValue() : readReg16(tpu, memory);
                u16 dividend = uA / uB;
                u16 remainder = uA % uB;

                if (isSignedOp) { // signed operation
                    s16 A = getSigned16(uA), B = getSigned16(uB);
                    dividend = remainder = 0;
                    dividend  |= A / B;
                    remainder |= A % B;
                }

                // move value & update flags
                bool isCarry = remainder == 0;
                tpu.moveToRegister(Register::AX, dividend);
                tpu.moveToRegister(Register::DX, remainder);
                setFlags(tpu, isCarry, getParity(dividend), dividend == 0, dividend & 0x8000, isCarry);
                break;
            }
            default: throw std::invalid_argument("Invalid MOD byte for operation: div/sdiv.");
        }
    }

    void processCMP(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get MOD byte
        const u8 argsFormat = mod.getValue() & 7;
        const bool isSignedOp = mod.getValue() & 8;

        // get first operand (even MOD-bytes are reg8s first)
        Register regA = (argsFormat & 1) ? getReg16(tpu, memory) : getReg8(tpu, memory);

        // get operands
        switch (argsFormat) {
            case 0:   // reg8, imm8
            case 2: { // reg8, reg8
                u8 uA = tpu.readRegister8(regA).getValue();
                u8 uB = (argsFormat == 2) ? readReg8(tpu, memory) : tpu.readByte(memory).getValue();
                u8 diff8 = uA - uB;
                bool isBorrow = uB > uA;

                if (isSignedOp) { // signed operation
                    s8 A = getSigned8(uA), B = getSigned8(uB);
                    diff8 = 0;
                    diff8 |= A - B; // copy bits (don't trust typecasts)
                    isBorrow = B > A;
                }

                // store result & update flags
                tpu.moveToRegister( regA, diff8 );
                setFlags(tpu, isBorrow, getParity(diff8), diff8 == 0, diff8 & 0x80, isBorrow);
                break;
            }
            case 1:   // reg16, imm16
            case 3: { // reg16, reg16
                u16 uA = tpu.readRegister16(regA).getValue();
                u16 uB = (argsFormat == 3) ? readReg16(tpu, memory) : tpu.readWord(memory).getValue();
                u16 diff16 = uA - uB;
                bool isBorrow = uB > uA;

                if (isSignedOp) { // signed operation
                    s16 A = getSigned16(uA), B = getSigned16(uB);
                    diff16 = 0;
                    diff16 |= A - B; // copy bits (don't trust typecasts)
                    isBorrow = B > A;
                }

                // store result & update flags
                tpu.moveToRegister( regA, diff16 );
                setFlags(tpu, isBorrow, getParity(diff16), diff16 == 0, diff16 & 0x8000, isBorrow);
                break;
            }
            default: throw std::invalid_argument("Invalid MOD byte for operation: cmp/scmp.");
        }
    }

    void processBUF(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get operands
        u16 value;
        const u8 argsFormat = mod.getValue() & 7;
        const bool is16Bit = argsFormat & 1; // 1 or 3
        switch (argsFormat) {
            case 0: value = tpu.readByte(memory).getValue(); break; // imm8
            case 1: value = tpu.readWord(memory).getValue(); break; // imm16
            case 2: value = readReg8(tpu, memory); break; // reg8
            case 3: value = readReg16(tpu, memory); break; // reg16
            default: throw std::invalid_argument("Invalid MOD byte for operation: buf.");
        }

        // update flags
        setFlags(tpu, 0, getParity(value), value == 0, value & (is16Bit ? 0x8000 : 0x80), 0);
    }

    void processANDORXOR(TPU& tpu, Memory& memory, u8 opCode) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get MOD byte
        const u8 argsFormat = mod.getValue() & 7;

        // get first operand (even MOD-bytes are reg8s first)
        Register regA = (argsFormat & 1) ? getReg16(tpu, memory) : getReg8(tpu, memory);
        switch (argsFormat) {
            case 0:   // reg8, imm8
            case 2: { // reg8, reg8
                u8 uA = tpu.readRegister8(regA).getValue();
                u8 uB = (argsFormat == 2) ? readReg8(tpu, memory) : tpu.readByte(memory).getValue();
                u8 result = opCode == OPCode::AND ? (uA & uB) : (opCode == OPCode::OR ? (uA | uB) : (uA ^ uB));

                // store result & update flags
                tpu.moveToRegister( regA, result );
                tpu.setFlag(PARITY, getParity(result));
                tpu.setFlag(ZERO, result == 0);
                tpu.setFlag(SIGN, result & 0x80);
                break;
            }
            case 1:   // reg16, imm16
            case 3: { // reg16, reg16
                u16 uA = tpu.readRegister16(regA).getValue();
                u16 uB = (argsFormat == 3) ? readReg16(tpu, memory) : tpu.readWord(memory).getValue();
                u16 result = opCode == OPCode::AND ? (uA & uB) : (opCode == OPCode::OR ? (uA | uB) : (uA ^ uB));

                // store result & update flags
                tpu.moveToRegister( regA, result );
                tpu.setFlag(PARITY, getParity(result));
                tpu.setFlag(ZERO, result == 0);
                tpu.setFlag(SIGN, result & 0x8000);
                break;
            }
            default: {
                switch (opCode) {
                    case OPCode::AND: throw std::invalid_argument("Invalid MOD byte for operation: and.");
                    case OPCode::OR: throw std::invalid_argument("Invalid MOD byte for operation: or.");
                    default: throw std::invalid_argument("Invalid MOD byte for operation: xor.");
                }
            }
        }
    }

    void processNOT(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get operands
        switch (mod.getValue() & 0b111) {
            case 0: { // reg8
                Register regA = getReg8(tpu, memory);
                tpu.moveToRegister( regA, ~tpu.readRegister8(regA).getValue() );
                break;
            }
            case 1: { // reg16
                Register regA = getReg16(tpu, memory);
                tpu.moveToRegister( regA, ~tpu.readRegister16(regA).getValue() );
                break;
            }
            default: throw std::invalid_argument("Invalid MOD byte for operation: not.");
        }
    }

    void processSHL(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get MOD byte
        const u8 argsFormat = mod.getValue() & 7;
        const bool isSignedOp = mod.getValue() & 8;

        // get first operand (even MOD-bytes are reg8s first)
        Register regA = (argsFormat & 1) ? getReg16(tpu, memory) : getReg8(tpu, memory);

        // get operands
        switch (argsFormat) {
            case 0:   // reg8, imm8
            case 2: { // reg8, reg8
                u8 A = tpu.readRegister8(regA).getValue();
                u8 numShifts = (argsFormat == 2) ? readReg8(tpu, memory) : tpu.readByte(memory).getValue();
                u8 value = A << std::min((int)numShifts, 8);
                if (isSignedOp) value |= A & 0x80; // re-add sign bit

                bool isCarry = (A << (std::min((int)numShifts, 8) - 1)) & 0x80; // holds last bit shifted
                bool isOverflow = ((A & 0x80) != (value & 0x80)); // set if sign bit changes

                // store result & update flags
                tpu.moveToRegister( regA, value );
                setFlags(tpu, isCarry, getParity(value), value == 0, value & 0x80, isOverflow);
                break;
            }
            case 1:   // reg16, imm16
            case 3: { // reg16, reg16
                u16 A = tpu.readRegister16(regA).getValue();
                u16 numShifts = (argsFormat == 2) ? readReg16(tpu, memory) : tpu.readWord(memory).getValue();
                u16 value = A << std::min((int)numShifts, 16);

                if (isSignedOp) value |= A & 0x8000; // re-add sign bit
                bool isCarry = (A << (std::min((int)numShifts, 16) - 1)) & 0x8000; // holds last bit shifted
                bool isOverflow = ((A & 0x8000) != (value & 0x8000)); // set if sign bit changes

                // store result & update flags
                tpu.moveToRegister( regA, value );
                setFlags(tpu, isCarry, getParity(value), value == 0, value & 0x8000, isOverflow);
                break;
            }
            default: throw std::invalid_argument("Invalid MOD byte for operation: shl/sshl.");
        }
    }

    void processSHR(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get MOD byte
        const u8 argsFormat = mod.getValue() & 7;
        const bool isSignedOp = mod.getValue() & 8;

        // get first operand (even MOD-bytes are reg8s first)
        Register regA = (argsFormat & 1) ? getReg16(tpu, memory) : getReg8(tpu, memory);

        // get operands
        switch (argsFormat) {
            case 0:   // reg8, imm8
            case 2: { // reg8, reg8
                u8 A = tpu.readRegister8(regA).getValue();
                u8 numShifts = (argsFormat == 2) ? readReg8(tpu, memory) : tpu.readByte(memory).getValue();
                u8 value = A >> std::min((int)numShifts, 8);
                if (isSignedOp) value |= A & 0x80; // re-add sign bit

                bool isCarry = (A >> (std::min((int)numShifts, 8) - 1)) & 0x80; // holds last bit shifted
                bool isOverflow = false; // always 0 for shr

                // store result & update flags
                tpu.moveToRegister( regA, value );
                setFlags(tpu, isCarry, getParity(value), value == 0, value & 0x80, isOverflow);
                break;
            }
            case 1:   // reg16, imm16
            case 3: { // reg16, reg16
                u16 A = tpu.readRegister16(regA).getValue();
                u16 numShifts = (argsFormat == 2) ? readReg16(tpu, memory) : tpu.readWord(memory).getValue();
                u16 value = A >> std::min((int)numShifts, 16);

                if (isSignedOp) value |= A & 0x8000; // re-add sign bit
                bool isCarry = (A >> (std::min((int)numShifts, 16) - 1)) & 1; // holds last bit shifted
                bool isOverflow = false; // always 0 for shr

                // store result & update flags
                tpu.moveToRegister( regA, value );
                setFlags(tpu, isCarry, getParity(value), value == 0, value & 0x8000, isOverflow);
                break;
            }
            default: throw std::invalid_argument("Invalid MOD byte for operation: shr/sshr.");
        }
    }
}