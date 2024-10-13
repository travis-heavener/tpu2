#ifndef __KERNEL_HPP
#define __KERNEL_HPP

// defines kernel interface functions exposed to the TPU

#include "../util/globals.hpp"

// used to start & stop all kernel boot processes
void startKernel();
void killKernel();

/****************************************************/
/*           memory allocation functions            */
/****************************************************/

class HeapFrag {
    public:
        HeapFrag(u16 size, u16 address, HeapFrag* pPrev, HeapFrag* pNext) : size(size), address(address), pPrev(pPrev), pNext(pNext) {};
        ~HeapFrag() { if (pNext != nullptr) delete pNext; };

        void allocate(u16 size);
        void free();

        void mergeRight();

        u16 size;
        u16 address;
        bool isFree = true;
        HeapFrag* pPrev = nullptr;
        HeapFrag* pNext = nullptr;
};

void initHeap(); // initializes the heap
void resetHeap(); // frees the heap

// allocates a given number of bytes on the stack, or T_NULL if failed
u16 heapAlloc(u16 size);

// attempts to reallocate the address given with a new size, or returns T_NULL if failed and doesn't affect the existing heap memory
u16 heapRealloc(u16 addr, u16 size);

// frees the chunk held at the given address
void heapFree(u16 addr);

#endif