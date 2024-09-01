#include <iostream>

#include "instructions.hpp"
#include "tpu.hpp"
#include "memory.hpp"

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
        switch (tpu.readRegister16(Register::AX).getValue()) {
            case Syscall::STDOUT: {
                u16 charPtr = tpu.readRegister16(Register::BX).getValue(); // get address for string start
                u16 length = tpu.readRegister16(Register::CX).getValue(); // get the length of string

                // load source index from BX and destination index from BX + length
                tpu.moveToRegister(Register::SI, charPtr);
                tpu.moveToRegister(Register::DI, charPtr + length);
                const u16 DI = tpu.readRegister16(Register::DI).getValue();
                tpu.sleep(); // sleep cycle since a handful of registers have just been written and read

                while (tpu.readRegister16(Register::SI).getValue() != DI) {
                    std::cout << (char)memory[tpu.readRegister16(Register::SI)++].getValue() << std::flush;
                    tpu.sleep(); // sleep between writes
                }
                break;
            }
        }
    }

    void processJMP(TPU& tpu, Memory& memory) {
        // determine operands from mod byte
        Byte mod = tpu.readByte(memory);
        tpu.sleep(); // wait since TPU has to process mod byte

        // get operands
        switch (mod.getValue() & 0b111) {
            case 0: { // Moves the instruction pointer to the specified 16-bit memory address.
                tpu.moveToRegister(Register::IP, tpu.readWord(memory).getValue());
                break;
            }
            case 1: { // Moves the instruction pointer to the specified 16-bit memory address, if the zero flag (ZF) is set.
                if (tpu.getFlag(ZERO)) {
                    tpu.moveToRegister(Register::IP, tpu.readWord(memory).getValue());
                }
                break;
            }
            case 2: { // Moves the instruction pointer to the specified 16-bit memory address, if the zero flag (ZF) is cleared.
                if (!tpu.getFlag(ZERO)) {
                    tpu.moveToRegister(Register::IP, tpu.readWord(memory).getValue());
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
            case 0: { // Move imm8 into address in memory
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
            case 2: { // Move imm8 into 8-bit register
                u8 opA = tpu.readByte(memory).getValue();
                tpu.moveToRegister(
                    getRegister8FromCode(opA), tpu.readByte(memory).getValue()
                );
                break;
            }
            case 3: { // Move imm16 into 16-bit register
                u8 opA = tpu.readByte(memory).getValue();
                tpu.moveToRegister(
                    getRegister16FromCode(opA), tpu.readWord(memory).getValue()
                );
                break;
            }
            case 4: { // Move 8-bit value from memory address into 8-bit register.
                u8 opA = tpu.readByte(memory).getValue();
                u16 addr = tpu.readWord(memory).getValue();
                tpu.moveToRegister(
                    getRegister8FromCode(opA), memory[addr].getValue()
                );
                break;
            }
            case 5: { // Move value between 8-bit registers.
                u8 opA = tpu.readByte(memory).getValue();
                u8 opB = tpu.readByte(memory).getValue();
                tpu.moveToRegister(
                    getRegister8FromCode(opA),
                    tpu.readRegister8(getRegister8FromCode(opB)).getValue()
                );
                break;
            }
            case 6: { // Move value between 16-bit registers.
                u8 opA = tpu.readByte(memory).getValue();
                u8 opB = tpu.readByte(memory).getValue();
                tpu.moveToRegister(
                    getRegister16FromCode(opA),
                    tpu.readRegister16(getRegister16FromCode(opB)).getValue()
                );
                break;
            }
            default: {
                throw std::invalid_argument("Invalid MOD byte for operation: mov.");
                break;
            }
        }
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
}