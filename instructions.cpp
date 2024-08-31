#include "instructions.hpp"
#include "tpu.hpp"
#include "memory.hpp"

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
            case 7: case 8: {
                throw std::invalid_argument("Invalid MOD byte for operation: mov.");
                break;
            }
        }
    }
}