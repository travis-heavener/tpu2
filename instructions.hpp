#ifndef __INSTRUCTIONS_HPP
#define __INSTRUCTIONS_HPP

#include "tpu.hpp"
#include "memory.hpp"

// abstraction from TPU.cpp to make processing instructions more tidy
namespace instructions {
    void processMOV(TPU& tpu, Memory& memory);
};

#endif