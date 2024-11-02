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

#define HEAP_FREE   0
#define HEAP_USED   1

// Used to initialize a new heap.
void heap_init() {
    // Initialize the heap
    uint_8* pHeap = HEAP_START;

    // Store the size of this block
    pHeap[0] = ((uint_16)(HEAP_SIZE - 3)) & 0xFF;
    pHeap[1] = ((uint_16)(HEAP_SIZE - 3) >> 8) & 0xFF;

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
    if (size > HEAP_SIZE - 3 || size == 0) {
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
                pHeap[i] = size & 0xFF;
                pHeap[i + 1] = (size >> 8) & 0xFF;

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

    while (i < HEAP_SIZE) {
        // Check if the address matches
        if (i + HEAP_START + 3 == addr) {
            // Mark the block as free
            pHeap[i + 2] = HEAP_FREE;
            return;
        }

        // Otherwise, jump to next block
        uint_16 blockSize = ((uint_16)pHeap[i+1] << 8) | pHeap[i];
        i = i + blockSize + 3;
    }
}

/**
 * Main kernel entry point.
 */

int main() {
    // Initialize heap
    heap_init();

    // Test allocate a block
    uint_16* addr;
    __load_CX(HEAP_SIZE-3);
    heap_alloc();
    addr = __read_DX();
    uint_8* pHeap = HEAP_START;

    // Free the block
    __load_BX(addr);
    heap_free();


    __load_CX(19);
    heap_alloc();
    addr = __read_DX();
    uint_16* p = addr;

   

    __load_CX(19);
    heap_alloc();
    addr = __read_DX();

    __load_BX(p);
    heap_free();


    __load_CX(14);
    heap_alloc();
    addr = __read_DX();

    __load_CX(1);
    heap_alloc();
    addr = __read_DX();

    return addr;
    // Exit success
    return EXIT_SUCCESS;
}