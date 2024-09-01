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

#define CLOCK_FREQ_HZ 5

int main() {
    // initialize the processor & memory
    TPU tpu(CLOCK_FREQ_HZ);
    Memory memory;

    // load a string into memory
    u16 stringStart = 0xFF10;
    std::string text("Hello world!\n");
    for (size_t i = 0; i < text.length(); i++) {
        memory[stringStart + i] = text[i];
    }
    
    // load program into memory
    u16 index = 0;

    // load syscall data for printing to STDERR
    
    // syscall type
    memory[index++] = OPCode::MOV; // instruction
    memory[index++] = 3; // MOD byte
    memory[index++] = Register::AX; // register code
    memory[index++] = Syscall::STDERR; // imm8
    memory[index++] = 0x00;
    
    // start address
    memory[index++] = OPCode::MOV; // instruction
    memory[index++] = 3; // MOD byte
    memory[index++] = Register::BX; // register code
    memory[index++] = stringStart & 0xFF; // imm16 lower half
    memory[index++] = (stringStart & 0xFF00) >> 8; // imm16 upper half

    // string length
    memory[index++] = OPCode::MOV; // instruction
    memory[index++] = 3; // MOD byte
    memory[index++] = Register::CX; // register code
    memory[index++] = text.length(); // imm8
    memory[index++] = 0x00;

    // make syscall
    memory[index++] = OPCode::SYSCALL;

    // stop the clock
    memory[index++] = HLT;

    // start the CPU's clock and wait
    tpu.start(memory);
    return 0;
}