#ifndef __WORD_HPP
#define __WORD_HPP

#include "byte.hpp"

class Word {
    public:
        /* Returns the corresponding ascending bit from the current word, either 0 or 1, stored as an unsigned char. */
        unsigned char operator[](unsigned char i) const { return (i > 7) ? byteH[i-8] : byteL[i]; };
        Word& operator=(unsigned short n) { this->byteH = (n & 0xFF00) >> 8; this->byteL = n & 0xFF; return *this; };
        Word& operator=(Word& n) { return this->operator=(n.getValue()); };

        /* Get the value of the Word as an unsigned short. */
        unsigned short getValue() const { return (((unsigned short)this->byteH.getValue()) << 8) | this->byteL.getValue(); };
    private:
        Byte byteH = 0, byteL = 0;
};

#endif