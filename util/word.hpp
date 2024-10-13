#ifndef __WORD_HPP
#define __WORD_HPP

#include "byte.hpp"
#include "globals.hpp"

class Word {
    public:
        // copy constructor
        Word();
        Word(u16);
        Word(const Byte& A, const Byte& B) : byteL(A), byteH(B) {};
        Word(Word& word) : byteL(word.byteL), byteH(word.byteH) {};
        
        /* Returns the corresponding ascending bit from the current word, either 0 or 1, stored as an unsigned char. */
        u8 operator[](u8) const;
        Word& operator=(u16);
        Word& operator=(Word& n) { return this->operator=(n.getValue()); };
        
        // post-inc/dec
        Word operator++(int);
        Word operator--(int);

        // shorthands
        void setUpper(u16 val) { byteH = val; };
        void setLower(u16 val) { byteL = val; };
        const Byte& getUpper() const { return byteH; }
        const Byte& getLower() const { return byteL; }

        /* Get the value of the Word as an u16. */
        u16 getValue() const;
        void setValue(u16);
    private:
        Byte byteL, byteH;
};

std::ostream& operator<<(std::ostream&, const Word&);

#endif