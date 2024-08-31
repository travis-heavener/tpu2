#include "tpu.hpp"

void TPU::reset() {
    // clear registers
    AX = BX = CX = DX = SP = BP = SI = DI = 0;

    // clear flags
    FLAGS = 0;
}