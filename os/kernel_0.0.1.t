/**
 * The official kernel for the TOS
 * 
 * @author Travis Heavener
 */

#include <stdio.t>

#define uint_16 unsigned int
#define uint_8 unsigned char

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define NULL 0

/**
 * Create a heap memory allocator.
 */

#define HEAP_START  (uint_16)0x3800
#define HEAP_END    (uint_16)0xFFFF
#define HEAP_SIZE   (uint_16)0xC800

#define HEAP_FREE   0
#define HEAP_USED   1

// Used to initialize a new heap.
void heap_init() {
    // Initialize the heap
    uint_8* pHeap = HEAP_START;

    // Store the size of this block
    *(uint_16*)(pHeap) = 0xC800 - 3;
    // pHeap[0] = 253; // Precompiled: ((uint_16)(HEAP_SIZE - 3)) & 0xFF;
    // pHeap[1] = 199; // Precompiled: ((uint_16)(HEAP_SIZE - 3) >> 8) & 0xFF;

    // Mark this block as free
    pHeap[2] = HEAP_FREE;
}

// Used to allocate a new block on the heap.
// Register CX: The size of allocation
// Register DX: The allocated address, or NULL
void heap_alloc() {
    // Grab alloc size from CX
    const uint_16 size = __read_CX();

    // Return NULL if the size is larger than the heap or is zero
    if (size > HEAP_SIZE - 3 || !size) {
        __load_DX( NULL );
        return;
    }

    // Declare the heap pointer
    uint_8* pHeap = HEAP_START;

    // Find the next available free block
    uint_16 i = 0;

    while (i < HEAP_SIZE) {
        // Check if the current block is free and is large enough
        uint_8 isFree = pHeap[i+2] == HEAP_FREE;
        uint_16 blockSize = ((uint_16)pHeap[i+1] << 8) | pHeap[i];

        if (isFree && (blockSize == size || (blockSize >= size + 3))) {
            // Mark block as in use
            pHeap[i + 2] = HEAP_USED;

            // If more space is in the block, create a new free block after it
            if (blockSize > size) {
                // Update this block's size
                *(uint_16*)(pHeap+i) = size;
                // pHeap[i] = size & 0xFF;
                // pHeap[i + 1] = (size >> 8) & 0xFF;

                // Create new block
                uint_16 remainingSize = blockSize - size - 3; // 3 bytes for metadata

                // Set new block's size & mark as free
                pHeap[i + size + 3] = remainingSize & 0xFF;
                pHeap[i + size + 4] = (remainingSize >> 8) & 0xFF;
                pHeap[i + size + 5] = HEAP_FREE;
            }

            // Return the address in DX
            __load_DX( i + HEAP_START + 3 );
            return;
        }

        // Otherwise, not a match, so check next block
        i = i + blockSize + 3;
    }

    // No block found, return NULL in DX
    __load_DX( NULL );
}

// Used to free a block on the heap by its address.
// Register BX: The allocated address
void heap_free() {
    // Grab address from BX
    const uint_16 addr = __read_BX();

    // Return if the address is outside the heap
    if (addr < HEAP_START || addr > HEAP_END) {
        return;
    }

    // Declare the heap pointer
    uint_8* pHeap = HEAP_START;

    // Find the block
    uint_16 i = 0;
    uint_16 prevSize = 0;

    while (i < HEAP_SIZE) {
        // Get the size of this block
        uint_16 blockSize = ((uint_16)pHeap[i+1] << 8) | pHeap[i];

        // Check if the address matches
        if (i + HEAP_START + 3 == addr) {
            // Mark the block as free
            pHeap[i + 2] = HEAP_FREE;

            // Attempt to coalesce heap blocks to prevent fragmentation

            // Check if next block exists
            if (i + blockSize + 5 < HEAP_SIZE) {
                // Check if next block is free
                if (pHeap[i + blockSize + 5] == HEAP_FREE) {
                    // Get size of next block
                    uint_16 nextBlockSize = ((uint_16)pHeap[i+blockSize+4] << 8) | pHeap[i+blockSize+3];

                    // Merge into this one by updating own size
                    blockSize = blockSize + nextBlockSize + 3; // Ignore next block's metadata
                    pHeap[i] = (blockSize) & 0xFF;
                    pHeap[i + 1] = (blockSize >> 8) & 0xFF;
                }
            }

            // Check if the previous block exists
            if (prevSize > 0) {
                // Check if previous block is free
                uint_16 prevIndexStart = i - prevSize - 3;
                if (pHeap[prevIndexStart + 2] == HEAP_FREE) {
                    // Update previous block's size by this one
                    prevSize = prevSize + blockSize + 3; // Ignore this block's metadata
                    pHeap[prevIndexStart] = (prevSize) & 0xFF;
                    pHeap[prevIndexStart+1] = (prevSize >> 8) & 0xFF;
                }
            }
            return;
        }

        // Otherwise, jump to next block
        i = i + blockSize + 3;
        prevSize = blockSize;
    }
}

/**
 * Main kernel entry point.
 */

int main() {
    // Initialize heap
    heap_init();

    // ALSO NOT WORKING
    __load_CX(12);
    heap_alloc();
    return __read_DX();

    // Exit success
    return EXIT_SUCCESS;
}