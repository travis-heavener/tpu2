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

int main() {
    // initialize the processor & memory
    TPU tpu;
    Memory memory;

    // write a byte into memory
    memory[0xFFFF] = 0x1F;
    std::cout << memory[0xFFFF] << '\n';
    return 0;
}