/**
 * The "Standard Library" for the T Language.
 * 
 * @author: Travis Heavener
 */

#define NULL 0

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

// shorthand types
#define uint_16 unsigned int
#define int_16 signed int
#define uint_8 unsigned char
#define int_8 signed char

/********* DYNAMIC HEAP MEMORY ALLOCATION *********/

void* malloc(const int size) {
    asm("movw AX, 0x04");   // specify syscall type
    __load_CX(size);        // load the requested size into CX
    asm("syscall");         // invoke kernel malloc function
    return __read_DX();     // return the address stored in DX
}

void* realloc(void* addr, const int size) {
    asm("movw AX, 0x05");   // specify syscall type
    __load_BX(addr);        // load the existing allocation address into BX
    __load_CX(size);        // load the requested size into CX
    asm("syscall");         // invoke kernel malloc function
    return __read_DX();     // return the address stored in DX
}

void free(void* addr) {
    asm("movw AX, 0x06");   // specify syscall type
    __load_BX(addr);        // load the existing allocation address into BX
    asm("syscall");         // invoke kernel malloc function
}