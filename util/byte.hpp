#ifndef __BYTE_HPP
#define __BYTE_HPP

#include <ostream>

typedef unsigned char u8;

class Byte {
    public:
        Byte() { this->data = 0; };
        Byte(u8 n) : data(n) {};

        // copy constructor
        Byte(const Byte& byte) : data(byte.data) {};
        
        /* Returns the corresponding bit from the current byte, either 0 or 1, stored as an u8 (u8). */
        u8 operator[](u8 i) const;

        Byte& operator=(u8 n);
        Byte& operator=(const Byte& byte) { this->data = byte.data; return *this; };
        u8 getValue() const { return this->data; };
    private:
        u8 data;
};

std::ostream& operator<<(std::ostream&, const Byte&);

#endif