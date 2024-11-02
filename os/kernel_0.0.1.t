/**
 * The official kernel for the TOS
 * 
 * @author Travis Heavener
 */

#include <stdlib.t>

/**
 * Create a heap memory allocator.
 */

#define HEAP_START  0x3800
#define HEAP_END    0xFFFF
#define HEAP_SIZE   HEAP_END - HEAP_START + 1

struct heap_t {
    struct heap_t* pNext;
    uint_16 size;
    uint_8 isFree;
};

// Used to initialize a new heap.
struct heap_t* heap_init() {
    // Initialize the heap
    struct heap_t* pHeap = (struct heap_t*)malloc( sizeof(struct heap_t) );

    // Set fields
    pHeap->pNext = NULL;
    pHeap->size = HEAP_SIZE;
    pHeap->isFree = 1;

    // Return heap
    return pHeap;
}

// Used to destroy the heap.
void heap_destroy(struct heap_t* pHeap) {
    // Free all reachable heap fragments
    struct heap_t* pNode = pHeap;

    // while (pNode != NULL) {
    //     struct heap_t* pNext = pNode->pNext;
    //     free(pNode);
    //     pNode = pNext;
    // }
}

/**
 * Main kernel entry point.
 */

int main() {
    // Initialize heap
    struct heap_t* pHeap = heap_init();

    // Destroy heap
    heap_destroy(pHeap);
    return EXIT_SUCCESS;
}