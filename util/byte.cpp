#include <ostream>

#include "byte.hpp"

u8 Byte::operator[](u8 i) const {
    if (i > 7)
        throw std::out_of_range("Index out of range");
    return (data & (1 << i)) >> i;
}

Byte& Byte::operator=(u8 n) {
    this->data = n;
    return *this;
}

std::ostream& operator<<(std::ostream& os, const Byte& byte) {
    // for hexadecimal representation
    unsigned char value = byte.getValue();
    switch ((value & 0xF0) >> 4) {
        case 10: os << 'A'; break;
        case 11: os << 'B'; break;
        case 12: os << 'C'; break;
        case 13: os << 'D'; break;
        case 14: os << 'E'; break;
        case 15: os << 'F'; break;
        default: os << (int)((value & 0xF0) >> 4); break;
    }

    switch (value & 0xF) {
        case 10: os << 'A'; break;
        case 11: os << 'B'; break;
        case 12: os << 'C'; break;
        case 13: os << 'D'; break;
        case 14: os << 'E'; break;
        case 15: os << 'F'; break;
        default: os << (int)(value & 0xF); break;
    }

    // for binary representation
    // for (int i = 7; i >= 0; --i) {
    //     os << (byte[i] ? '1' : '0');
    // }
    return os;
}