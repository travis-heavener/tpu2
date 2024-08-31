#include <iostream>

#include "tpu.hpp"
#include "memory.hpp"

/**
 * The TPU-2 (Terrible Processing Unit version 2) is an emulated 16-bit CPU.
 * This means that it's able to therefore address 2^16 (65536)
 * different addresses in memory.
 * 
 * NOTE (A):
 * This is heavily inspired by the Intel 8086 16-bit processor.
 * 
 * NOTE (B):
 * After having abandoned this project a few months back, I'm repurposing a
 * large amount of that code in this revival project.
*/

/**
 * NOTE: due to internal clock resolution, the thread sleep handles microseconds and thus any
 *  clock speed past 1 Mhz will cause the thread to sleep for 0 microseconds (basically not sleeping).
*/

#define CLOCK_FREQ_HZ 100000

int main() {
    // initialize the processor & memory
    TPU tpu(CLOCK_FREQ_HZ);
    Memory memory;

    // load program into memory
    u16 index = 0;

    // copy 0xFA into 8-bit register AH
    memory[index++] = OPCode::MOV; // instruction
    memory[index++] = 2; // MOD byte
    memory[index++] = Register::AH; // register code
    memory[index++] = 0xFA; // imm8
    
    // copy 0xB1 into memory at 0x8411
    memory[index++] = OPCode::MOV; // instruction
    memory[index++] = 0; // MOD byte
    memory[index++] = 0x11; // imm16 lower half as imm8 (little endian)
    memory[index++] = 0x84; // imm16 upper half as imm8
    memory[index++] = 0xB1; // imm8

    // copy 0x8411 into register AL
    memory[index++] = OPCode::MOV; // instruction
    memory[index++] = 4; // MOD byte
    memory[index++] = Register::AL; // register code
    memory[index++] = 0x11; // imm16 lower half as imm8 (little endian)
    memory[index++] = 0x84; // imm16 upper half as imm8

    // add AL to AH
    memory[index++] = OPCode::ADD; // instruction
    memory[index++] = 2; // MOD byte
    memory[index++] = Register::AH; // register code
    memory[index++] = Register::AL; // register code

    // copy register AH to 0x8412
    memory[index++] = OPCode::MOV; // instruction
    memory[index++] = 1; // MOD byte
    memory[index++] = 0x12; // imm16 lower half as imm8 (little endian)
    memory[index++] = 0x84; // imm16 upper half as imm8
    memory[index++] = Register::AH; // register code

    // copy flags to BX
    memory[index++] = OPCode::MOV; // instruction
    memory[index++] = 6; // MOD byte
    memory[index++] = Register::BX; // register code
    memory[index++] = Register::FLAGS; // register code

    // copy BL to 0x8413
    memory[index++] = OPCode::MOV; // instruction
    memory[index++] = 1; // MOD byte
    memory[index++] = 0x13; // imm16 lower half as imm8 (little endian)
    memory[index++] = 0x84; // imm16 upper half as imm8
    memory[index++] = Register::BL; // register code
    
    // copy BH to 0x8414
    memory[index++] = OPCode::MOV; // instruction
    memory[index++] = 1; // MOD byte
    memory[index++] = 0x14; // imm16 lower half as imm8 (little endian)
    memory[index++] = 0x84; // imm16 upper half as imm8
    memory[index++] = Register::BH; // register code

    // stop the clock
    memory[index++] = HLT;

    // start the CPU's clock and wait
    tpu.start(memory);

    std::cout << memory[0x8412] << '\n';
    std::cout << Word(memory[0x8413], memory[0x8414]) << '\n';
    return 0;
}