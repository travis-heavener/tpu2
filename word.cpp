#include "word.hpp"

Word::Word() {
    this->byteH = 0;
    this->byteL = 0;
}

Word::Word(u16 value) {
    this->byteH = (value & 0xFF00) >> 8;
    this->byteL = value & 0x00FF;
}

u8 Word::operator[](u8 i) const {
    return (i > 7) ? byteH[i-8] : byteL[i];
}

Word& Word::operator=(u16 n) {
    this->byteH = (n & 0xFF00) >> 8;
    this->byteL = n & 0xFF;
    return *this;
}

Word Word::operator++(int) {
    Word copy(*this);
    this->operator=(this->getValue()+1);
    return copy;
}

Word Word::operator--(int) {
    Word copy(*this);
    this->operator=(this->getValue()-1);
    return copy;
}

u16 Word::getValue() const {
    return (((u16)byteH.getValue()) << 8) | byteL.getValue();
}

std::ostream& operator<<(std::ostream& os, const Word& word) {
    os << word.getUpper() << word.getLower();
    return os;
}