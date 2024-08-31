#ifndef __BYTE_HPP
#define __BYTE_HPP

#include <ostream>

class Byte {
    public:
        Byte() { this->data = 0; };
        Byte(unsigned char n) : data(n) {};
        
        /* Returns the corresponding bit from the current byte, either 0 or 1, stored as an unsigned char (unsigned char). */
        unsigned char operator[](unsigned char i) const;

        Byte& operator=(unsigned char n);
        unsigned char getValue() const { return this->data; };
    private:
        unsigned char data;
};

std::ostream& operator<<(std::ostream& os, const Byte& byte);

#endif