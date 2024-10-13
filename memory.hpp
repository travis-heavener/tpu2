#ifndef __MEMORY_HPP
#define __MEMORY_HPP

#include "util/globals.hpp"
#include "util/byte.hpp"
#include "util/word.hpp"

#define MAX_MEMORY 0xFFFF+1 // 2 ^ 16 addressable bytes

class Memory {
    public:
        Memory();
        ~Memory();
        void reset();
        Byte& operator[](u16 addr) const {  return *(pData + addr);  };
        Byte& operator[](Word addr) const {  return this->operator[](addr.getValue());  };
    private:
        Byte* pData;
};

#endif