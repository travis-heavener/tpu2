#ifndef __INSTRUCTIONS_HPP
#define __INSTRUCTIONS_HPP

#include "tpu.hpp"
#include "memory.hpp"

// abstraction from TPU.cpp to make processing instructions more tidy
namespace instructions {
    void executeSyscall(TPU& tpu);
    void processCALL(TPU& tpu);
    void processJMP(TPU& tpu, u8);
    void processRET(TPU& tpu);
    void processMOV(TPU& tpu, u8);
    void processLB(TPU& tpu);
    void processLW(TPU& tpu);
    void processSB(TPU& tpu);
    void processSW(TPU& tpu);
    void processPUSH(TPU& tpu, u8);
    void processPOP(TPU& tpu, u8);
    void processADD(TPU& tpu);
    void processSUB(TPU& tpu);
    void processMUL(TPU& tpu);
    void processDIV(TPU& tpu);
    void processCMP(TPU& tpu);
    void processBUF(TPU& tpu);
    void processANDORXOR(TPU& tpu, u8);
    void processNOT(TPU& tpu);
    void processSHL(TPU& tpu);
    void processSHR(TPU& tpu);
};

#endif