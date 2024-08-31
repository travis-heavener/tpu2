#include "memory.hpp"

Memory::Memory() {
    // initialize cleared heap memory
    this->pData = new Byte[MAX_MEMORY];

    this->reset();
}

Memory::~Memory() {
    // free all alloc'ed memory
    delete[] this->pData;
}

void Memory::reset() {
    // zero all values in memory
    for (int i = 0; i < MAX_MEMORY; i++)
        *(this->pData+i) = 0;
}