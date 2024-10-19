#include <fstream>
#include <iostream>

#include "util/globals.hpp"
#include "tpu.hpp"
#include "memory.hpp"
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

void loadProgramFromImage(const std::string& imagePath, TPU& tpu, Memory& memory) {
    // open the file
    std::ifstream imageHandle(imagePath);
    if (!imageHandle.is_open()) {
        std::cerr << "Failed to open disk image: " << imagePath << '\n';
        exit(1);
    }

    // write jump instruction from start of sector
    const u16 startPos = 128;
    u32 pos = startPos;
    const u16 partitionSize = 2048;
    imageHandle.seekg(pos, std::ios::beg);
    
    u16 textStartAddr = imageHandle.get();
    textStartAddr |= ((u16)imageHandle.get() << 8);
    memory[RESERVED_LOWER_ADDR] = OPCode::JMP; // JMP instruction
    memory[RESERVED_LOWER_ADDR+1] = 0; // MOD byte
    memory[RESERVED_LOWER_ADDR+2] = textStartAddr & 0xFF; // lower-half of address
    memory[RESERVED_LOWER_ADDR+3] = (textStartAddr >> 8) & 0xFF; // upper-half of address
    pos += 2;

    // write .data section
    for (u16 i = 0; pos < textStartAddr; ++i, ++pos) {
        memory[DATA_LOWER_ADDR + i] = imageHandle.get();
    }

    // write .text section
    for (u16 i = 0; pos < partitionSize + startPos; ++i, ++pos) {
        memory[RESERVED_LOWER_ADDR + i] = imageHandle.get();
    }

    // move the IP to the start of the reserved pool
    tpu.moveToRegister(Register::IP, RESERVED_LOWER_ADDR);

    // close the file
    imageHandle.close();
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Invalid usage: <executable> <image_path.dsk>\n";
        exit(1);
    }

    // initialize the processor & memory
    TPU tpu(CLOCK_FREQ_HZ);
    Memory memory;

    // load the OS from the disk image
    // first 2 KiB of the disk is partitioned for the OS
    const std::string imagePath( argv[1] );
    loadProgramFromImage(imagePath, tpu, memory);

    // start the kernel
    startKernel();

    try {
        // start the CPU's clock and wait
        tpu.start(memory);

        std::cout << tpu.readRegister16(Register::AX) << ' ' << tpu.readRegister16(Register::BX) << '\n';
        std::cout << tpu.readRegister16(Register::CX) << ' ' << tpu.readRegister16(Register::DX) << '\n';
        std::cout << (memory)[tpu.readRegister16(Register::SP).getValue()-1] << '\n';
        std::cout << tpu.readRegister16(Register::SP).getValue() << '\n';
        std::cout << "Flags: " << (s16)tpu.readRegister16(Register::FLAGS).getValue() << ".\n";

        // print exit status
        std::cout << "Program exited with status " << (s16)tpu.readRegister16(Register::ES).getValue() << ".\n";
    } catch (std::invalid_argument& e) {
        std::cerr << e.what() << '\n';
    }

    // kill the kernel
    killKernel();

    return 0;
}