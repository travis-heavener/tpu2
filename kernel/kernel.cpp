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
class HeapFrag {
    public:
        HeapFrag(u16 size, u16 address, HeapFrag* pNext) : size(size), address(address), pNext(pNext) {};
        ~HeapFrag() { if (pNext != nullptr) delete pNext; };
        u16 size;
        u16 address;
        HeapFrag* pNext = nullptr;
};

// initializes the heap
static HeapFrag* pHeap = nullptr;
void initHeap() {
    // create initial heap free fragment
    pHeap = new HeapFrag(HEAP_SIZE, HEAP_LOWER_ADDR, nullptr);
}

// frees the heap
void resetHeap() {
    delete pHeap;
}