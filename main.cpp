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

    // copy 0xF1 into 8-bit register AH
    memory[index++] = MOV; // instruction
    memory[index++] = 2; // MOD byte
    memory[index++] = Register::AH; // register code
    memory[index++] = 0xF1; // imm8

    // copy AX to CX
    memory[index++] = MOV; // instruction
    memory[index++] = 6; // MOD byte
    memory[index++] = Register::CX; // dest register
    memory[index++] = Register::AX; // source register

    // copy 8-bit register CH to memory address 0x8412
    memory[index++] = MOV; // instruction
    memory[index++] = 1; // MOD byte
    memory[index++] = 0x12; // imm16 lower half as imm8
    memory[index++] = 0x84; // imm16 upper half as imm8 (little endian)
    memory[index++] = Register::CL; // register code
    
    // copy 8-bit register CL to memory address 0x8413
    memory[index++] = MOV; // instruction
    memory[index++] = 1; // MOD byte
    memory[index++] = 0x13; // imm16 lower half as imm8
    memory[index++] = 0x84; // imm16 upper half as imm8
    memory[index++] = Register::CH; // register code

    // stop the clock
    memory[index++] = HLT;

    // start the CPU's clock and wait
    tpu.start(memory);

    std::cout << Word(memory[0x8412], memory[0x8413]) << '\n';
    return 0;
}