#include <filesystem>
#include <fstream>
#include <iostream>

#include "../memory.hpp"
#include "asm_loader.hpp"

/**
 * 
 * @author Travis Heavener
 * 
 * This program is used to load a specific .TPU program and generate its binary program, acting as the assembler.
 * 
 * These 64 KiB drives are composed of a reserved 128 bytes at the start, where each bit corresponds to whether or not the
 * corresponding sector from the other 1022 sectors (each 64 bytes) is free (0 is free, 1 is reserved).
 * 
 * The first two bytes of a binary indicate where the start of .text is (a jmp instruction to the main entry point).
 * Everything from byte #3 to byte N-1 (where byte N is the aforementioned jmp) is reserved for .data, which always has the same
 * addressing system across all programs (allowing .data addresses to be determined at assemble-time).
 * 
 */

#define SECTOR_SIZE 64
#define DRIVE_SIZE 64 * 1024

// the issue now is that i need a way to store literally where each program is, like a filesystem
// i need a naming system for files, since the bytes here don't mean anything unless I can name the programs and all that

// this is where the reserved partition comes in, the OS needs names for all the programs it knows, thus I need an index

/**
     * ^^^^^^^ the above has issues
     * X) need to find a way to store .data section into binary
     * 2) need to store program start index in the first 2 bytes of reserved memory
     * X) since one program can run at once, we can store things in .data as usual (it resets for each program)
     * X) ^^^^^ this might actually not be true if we need .data for the kernel/OS (but then again, we have reserved pool for this)
     * 5) most importantly, we need to store the end instruction index somewhere so we know how many sectors are needed
     */

// finds the next available sector start address on the given drive
u16 getAvailableSectorStart(Memory& image, const u16 sizeRequired) {
    // determine the number of consecutive sectors needed
    const u8 sectorsRequired = sizeRequired / SECTOR_SIZE;
    u16 sectorStart; // start of the found sector
    u16 sectorsAvailable = 0; // # of consecutive sectors

    // read first 128 bytes (entire sector map)
    for (u8 i = 0; i < 128; ++i) {
        const u8 mapChunk = image[i].getValue();

        // skip any full bytes (saves some time)
        if (mapChunk == 0xFF) continue;

        // check all 8 bits
        for (u8 j = 0; j < 8; ++j) {
            if ( !(mapChunk & (1 << j)) ) { // free chunk!
                if (sectorsAvailable > 0) { // consecutive free chunk
                    ++sectorsAvailable;
                } else { // new free chunk
                    sectorStart = (i * 8 + j) * SECTOR_SIZE; // skip by index * chunk size
                    
                    // check for sufficient consecutive sectors
                    sectorsAvailable = 1;
                }

                // check if the # of sectors needed is met
                if (sectorsAvailable == sectorsRequired) {
                    return sectorStart;
                }
            } else if (sectorsAvailable > 0) {
                // insufficient sectors at this address, look for another
                sectorsAvailable = 0;
            }
        }
    }

    // couldn't find a free address
    std::cerr << "Failed to find a free sector--drive is full.\n";
    exit(1);
}

int main(int argc, char* argv[]) {
    // load in the TPU file & target drive
    if (argc < 3) {
        std::cerr << "Invalid usage: <executable> <target.tpu> <drive> \n";
        return 1;
    }

    // load the program into temp memory
    Memory programMemory;
    const std::string inPath( argv[1] );

    u16 textStart = 0, dataStart = 0;
    u16 requiredSizeBytes = loadFileToMemory(inPath, programMemory, textStart, dataStart);
    u16 requiredSectorSize = requiredSizeBytes + (SECTOR_SIZE - (requiredSizeBytes % SECTOR_SIZE));

    // load the disk image to memory (it's 64 KiB so it has a negligible memory footprint :D)
    Memory image;

    const std::string drivePath( argv[2] );
    std::ifstream driveHandle( drivePath );
    if (!driveHandle.is_open()) {
        std::cerr << "Failed to open drive: " << drivePath << '\n';
        return 1;
    }

    u32 pos = 0;
    while (pos < DRIVE_SIZE) {
        image[pos++] = driveHandle.get();
    }

    // close the file handle
    driveHandle.close();

    // find the next available sector from the sector map
    u16 sectorStart = getAvailableSectorStart(image, requiredSectorSize);

    // update the sector map
    const u8 sectorIndex = sectorStart / SECTOR_SIZE;
    const u8 numSectorsUsed = requiredSectorSize / SECTOR_SIZE;
    for (u16 i = 0; i < numSectorsUsed; ++i) {
        // for each sector in the map, flip the bit to 1
        const u8 currentSectorIndex = sectorIndex + i;
        u8 currentByte = image[currentSectorIndex / 8].getValue();
        image[currentSectorIndex / 8] = currentByte | (1 << (currentSectorIndex % 8));
    }

    // starting at the free sector, move the program onto the disk
    // first two bytes mark where the .text section starts
    pos = sectorStart;
    image[pos++] = textStart & 0xFF; // lower-half of address
    image[pos++] = (textStart >> 8) & 0xFF; // upper-half of address
    
    // write data section
    const u16 dataSectionSize = (dataStart > textStart) ? (requiredSizeBytes - dataStart) : (textStart - dataStart);
    for (u16 i = 0; i < dataSectionSize; ++i) {
        image[pos++] = programMemory[dataStart + i].getValue();
    }

    // write text section
    const u16 textSectionSize = requiredSizeBytes - textStart;
    for (u16 i = 0; i < textSectionSize; ++i) {
        image[pos++] = programMemory[textStart + i].getValue();
    }

    // reopen drive file to overwrite in place
    std::ofstream outHandle( drivePath, std::ios::trunc );
    if (!outHandle.is_open()) {
        std::cerr << "Failed to reopen drive image for writing.\n";
        return 1;
    }

    // write the updated image
    for (u32 i = 0; i < DRIVE_SIZE; ++i) {
        outHandle << image[i].getValue();
    }

    // close files & replace drive with tmp
    outHandle.close();

    return 0;
}