#ifndef __MEMORY_HPP
#define __MEMORY_HPP

#include "byte.hpp"
#include "word.hpp"

#define MAX_MEMORY 0xFFFF+1 // 2 ^ 16 addressable bytes

class Memory {
    public:
        Memory();
        ~Memory();
        void reset();
        Byte& operator[](unsigned short addr) const {  return *(pData + addr);  };
        Byte& operator[](Word addr) const {  return this->operator[](addr.getValue());  };
    private:
        Byte* pData;
};

#endif