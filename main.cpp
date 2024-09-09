#include <iostream>

#include "tpu.hpp"
#include "memory.hpp"
#include "assembler.hpp"

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

#define CLOCK_FREQ_HZ 5000

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Invalid usage: <executable> path_to_file.tpu\n";
        exit(1);
    }
    
    // initialize the processor & memory
    TPU tpu(CLOCK_FREQ_HZ);
    Memory memory;

    // load test program to memory
    loadFileToMemory(argv[1], memory);

    // start the CPU's clock and wait
    tpu.start(memory);

    std::cout << tpu.readRegister16(Register::AX) << '\n';

    return 0;
}