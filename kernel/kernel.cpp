#include "kernel.hpp"

// used to start & stop all kernel boot processes
void startKernel() {
    initHeap();
}

void killKernel() {
    resetHeap();
}

/****************************************************/
/*           memory allocation functions            */
/****************************************************/

static HeapFrag* pHeap = nullptr;

void HeapFrag::allocate(u16 size) {
    // create new node
    HeapFrag* pNew = new HeapFrag(this->size - size, this->address + size, this, this->pNext);

    // update self
    this->pNext = pNew;
    this->size = size;
    this->isFree = false;
}

void HeapFrag::free() {
    this->isFree = true;

    // dissolve self into previous and next nodes, if possible
    if (pNext != nullptr && pNext->isFree)
        mergeRight(); // merge with next node
}

void HeapFrag::mergeRight() {
    // merge this chunk with the one after it (only called if both are free)
    HeapFrag* pNextOld = pNext;
    this->pNext = pNextOld->pNext;
    this->size += pNextOld->size;

    // update next next pointer
    if (pNextOld->pNext != nullptr) {
        pNextOld->pNext->pPrev = this;
        pNextOld->pNext = nullptr; // prevent destructor from destroying rest of heap
    }

    // free old next chunk
    delete pNextOld;
}

// initializes the heap
void initHeap() {
    // create initial heap free fragment
    pHeap = new HeapFrag(HEAP_SIZE, HEAP_LOWER_ADDR, nullptr, nullptr);
}

// allocates a given number of bytes on the stack, or T_NULL if failed
u16 heapAlloc(u16 size) {
    // if the size is zero, return T_NULL
    if (size == 0) return T_NULL;

    // attempt to find a free chunk with enough size
    HeapFrag* pNode = pHeap;
    while (pNode != nullptr) {
        // check this chunk
        if (!pNode->isFree || pNode->size < size) {
            pNode = pNode->pNext;
            continue;
        }

        // base case, enough size must exist so break apart this chunk
        pNode->allocate(size);
        return pNode->address;
    }

    // base case, no chunk was found, return T_NULL
    return T_NULL;
}

// attempts to reallocate the address given with a new size, or returns T_NULL if failed and doesn't affect the existing heap memory
u16 heapRealloc(u16 addr, u16 size) {
    // if the size is zero, return T_NULL
    if (size == 0) return T_NULL;

    // find the chunk, or return T_NULL if not exists
    HeapFrag* pNode = pHeap;
    while (pNode != nullptr && (pNode->address != addr || pNode->isFree)) {
        pNode = pNode->pNext; // go to next node
    }

    if (pNode == nullptr) return T_NULL;

    // if the size isn't changing, just return the address
    u16 retAddr;
    if (pNode->size == size) {
        retAddr = addr;
    } else if (pNode->size > size) { // the node exists, so attempt to resize it
        // allocate this and then merge the two chunks after this
        pNode->isFree = true;
        pNode->allocate(size);

        // merge the dangling next chunk with the one after, if possible
        if (pNode->pNext->pNext != nullptr && pNode->pNext->pNext->isFree)
            pNode->pNext->mergeRight();

        // return same address
        retAddr = addr;
    } else {
        // check next chunk for space
        if (pNode->pNext != nullptr && pNode->pNext->isFree && pNode->pNext->size + pNode->size >= size) {
            // the next node can be merged with this one
            pNode->mergeRight();
            
            // allocate space in pNode
            pNode->allocate(size);

            // return same address
            retAddr = addr;
        } else {
            // allocate elsewhere (passthrough to heapAlloc)
            retAddr = heapAlloc(size);

            // if a new chunk was found, free the old one
            if (retAddr != T_NULL)
                heapFree(addr);
        }
    }

    // return the new address
    return retAddr;
}

// frees the chunk held at the given address
void heapFree(u16 addr) {
    // find the chunk
    HeapFrag* pNode = pHeap;
    while (pNode != nullptr && pNode->address != addr) {
        pNode = pNode->pNext; // go to next node
    }

    // if a nullptr, the chunk couldn't be freed
    if (pNode != nullptr) {
        // free if not already freed
        if (!pNode->isFree) {
            pNode->free(); // free the node

            // if the previous node exists and is free, call its mergeRight method
            if (pNode->pPrev != nullptr && pNode->pPrev->isFree) {
                pNode->pPrev->mergeRight();
            }
        }
    }
}

// frees the heap
void resetHeap() {
    delete pHeap;
}