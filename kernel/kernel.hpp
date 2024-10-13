#ifndef __KERNEL_HPP
#define __KERNEL_HPP

// defines kernel interface functions exposed to the TPU

#include "../util/globals.hpp"

// used to start & stop all kernel boot processes
void startKernel();
void killKernel();

/****************************************************/
/*           memory allocation functions            */
/****************************************************/

void initHeap(); // initializes the heap
void resetHeap(); // frees the heap

#endif