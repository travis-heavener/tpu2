#include <iostream>

#include "util/globals.hpp"
#include "tpu.hpp"
#include "memory.hpp"
#include "asm_loader.hpp"
#include "kernel/kernel.hpp"

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

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Invalid usage: <executable> path_to_file.tpu\n";
        exit(1);
    }

    // initialize the processor & memory
    TPU tpu(CLOCK_FREQ_HZ);
    Memory memory;

    // start the kernel
    startKernel();

    try {
        // load test program to memory
        loadFileToMemory(argv[1], memory);

        // start the CPU's clock and wait
        tpu.start(memory);

        std::cout << tpu.readRegister16(Register::AX) << ' ' << tpu.readRegister16(Register::BX) << '\n';
        std::cout << tpu.readRegister16(Register::CX) << ' ' << tpu.readRegister16(Register::DX) << '\n';
        std::cout << (memory)[tpu.readRegister16(Register::SP).getValue()-1] << '\n';
        std::cout << tpu.readRegister16(Register::SP).getValue() << '\n';
        std::cout << "Flags: " << (short)tpu.readRegister16(Register::FLAGS).getValue() << ".\n";

        // print exit status
        std::cout << "Program exited with status " << (short)tpu.readRegister16(Register::ES).getValue() << ".\n";
    } catch (std::invalid_argument& e) {
        std::cerr << e.what() << '\n';
    }

    // kill the kernel
    killKernel();

    return 0;
}