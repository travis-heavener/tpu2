/**
 * Defines a Display adapter class for the TPU project.
 */

#include "util/globals.hpp"
#include "memory.hpp"

class TPU; // fwd dec

class Display {
    public:
        Display();
        ~Display();
        void renderFrame(TPU&, Memory&);
    private:
        u8 readPixel(u8 r, u8 c) { return pDisplay[(r * DISPLAY_WIDTH) + c]; };
        u8* pDisplay;
};