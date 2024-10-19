#ifndef __INSTRUCTIONS_HPP
#define __INSTRUCTIONS_HPP

#include "tpu.hpp"
#include "memory.hpp"

// abstraction from TPU.cpp to make processing instructions more tidy
namespace instructions {
    void executeSyscall(TPU& tpu, Memory& memory);
    void processCALL(TPU& tpu, Memory& memory);
    void processRET(TPU& tpu, Memory& memory);
    void processJMP(TPU& tpu, Memory& memory);
    void processMOV(TPU& tpu, Memory& memory);
    void processMOVW(TPU& tpu, Memory& memory);
    void processPUSH(TPU& tpu, Memory& memory);
    void processPOP(TPU& tpu, Memory& memory);
    void processPOPW(TPU& tpu, Memory& memory);
    void processADD(TPU& tpu, Memory& memory);
    void processSUB(TPU& tpu, Memory& memory);
    void processMUL(TPU& tpu, Memory& memory);
    void processDIV(TPU& tpu, Memory& memory);
    void processCMP(TPU& tpu, Memory& memory);
    void processBUF(TPU& tpu, Memory& memory);
    void processAND(TPU& tpu, Memory& memory);
    void processOR(TPU& tpu, Memory& memory);
    void processXOR(TPU& tpu, Memory& memory);
    void processNOT(TPU& tpu, Memory& memory);
    void processSHL(TPU& tpu, Memory& memory);
    void processSHR(TPU& tpu, Memory& memory);
};

#endif