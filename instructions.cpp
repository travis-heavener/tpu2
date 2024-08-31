#include "instructions.hpp"
#include "tpu.hpp"
#include "memory.hpp"

constexpr bool getParity(unsigned short n) {
    bool parity = false;
    for (unsigned char i = 0; i < 8; i++)
        if ((n >> i) & 1)
            parity = !parity;
    return parity;
}

namespace instructions {
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
                throw std::invalid_argument("Invalid MOD byte for operation: mov.");
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
                throw std::invalid_argument("Invalid MOD byte for operation: mov.");
                break;
            }
        }
    }
}