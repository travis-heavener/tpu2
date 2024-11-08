/**
 * The official kernel for the TOS
 * 
 * @author Travis Heavener
 */

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
#define USABLE_HEAP_SIZE   (uint_16)0xC800 - 3

#define HEAP_FREE   0
#define HEAP_USED   1

// Used to allocate a new block on the heap.
// Register CX: The size of allocation
// Register DX: The allocated address, or NULL
void heap_alloc() {
    // Grab alloc size from CX
    const uint_16 size = __read_CX();

    // Return NULL if the size is larger than the heap or is zero
    if (size > USABLE_HEAP_SIZE || !size) {
        __load_DX( NULL );
        return;
    }

    // Declare the heap pointer
    uint_8* pHeap = HEAP_START;

    // Find the next available free block
    uint_16 i = 0;

    while (i < HEAP_SIZE) {
        // Check if the current block is free and is large enough
        uint_16 blockSize = *(uint_16*)(pHeap+i);

        if (pHeap[i+2] == HEAP_FREE && (blockSize == size || (blockSize >= size + 3))) {
            // Mark block as in use
            pHeap[i+2] = HEAP_USED;

            // If more space is in the block, create a new free block after it
            if (blockSize > size) {
                // Update this block's size
                *(uint_16*)(pHeap+i) = size;

                // Create new block
                uint_16 nextStatusIndex = i + size + 3;
                *((uint_16*)(pHeap + nextStatusIndex)) = blockSize - size - 3; // 3 bytes for metadata
                pHeap[nextStatusIndex+2] = HEAP_FREE;
            }

            // Return the address in DX
            __load_DX( i + HEAP_START + 3 );
            return;
        }

        // Otherwise, not a match, so check next block
        i += blockSize + 3;
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
        uint_16 blockSize = *(uint_16*)(pHeap+i);

        // Check if the address matches
        if (i + HEAP_START + 3 == addr) {
            // Mark the block as free
            pHeap[i + 2] = HEAP_FREE;

            // Attempt to coalesce heap blocks to prevent fragmentation

            // Check if next block exists
            uint_16 nextStatusIndex = i + blockSize + 5;
            if (nextStatusIndex < HEAP_SIZE) {
                // Check if next block is free
                if (pHeap[nextStatusIndex] == HEAP_FREE) {
                    // Get size of next block
                    uint_16 nextBlockSize = *(uint_16*)(pHeap + nextStatusIndex - 2);

                    // Merge into this one by updating own size
                    // Ignore next block's metadata
                    *(uint_16*)(pHeap+i) = blockSize += nextBlockSize + 3;
                }
            }

            // Check if the previous block exists
            if (prevSize > 0) {
                // Check if previous block is free
                uint_16 prevIndexStart = i - prevSize - 3;
                if (pHeap[prevIndexStart + 2] == HEAP_FREE) {
                    // Update previous block's size by this one
                    // Ignore this block's metadata
                    *(uint_16*)(pHeap + prevIndexStart) = prevSize += blockSize + 3;
                }
            }
            return;
        }

        // Otherwise, jump to next block
        i += blockSize + 3;
        prevSize = blockSize;
    }
}

/**
 * Main kernel entry point.
 */

int main() {
    // Initialize heap
    *(uint_16*)(HEAP_START) = USABLE_HEAP_SIZE; // Store size of this block
    *(uint_8*)(HEAP_START+2) = HEAP_FREE; // Mark as free

    // Exit success
    return EXIT_SUCCESS;
}