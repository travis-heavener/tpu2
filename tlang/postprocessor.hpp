#ifndef __POSTPROCESSOR_HPP
#define __POSTPROCESSOR_HPP

#include <string>

typedef struct post_process_opts {
    bool minify; // removes any unnecessary whitespace (ex. leadings tabs)
    bool mergeImm8Pushes; // combines any consecutive imm8 push operations into imm16 pushw operations
    bool reducePushPopsToRegisters; // combines any push/pop operations between registers to mov/movw instructions
    bool dissolvePops; // combines any consecutive target-less pop/popw instructions to just subtracting from the SP
    bool stripComments; // removes any comments from the input file
} post_process_opts;

// an optional postprocessor for reducing the number of instructions of a given TPU file
void postProcess(const std::string&, const post_process_opts&);

#endif