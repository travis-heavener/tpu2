/**
 * Extension of the "T Standard Library" for IO support.
 */

#include <string.t>

int dummy(char* str) {
    return strlen(str);
}

void print(char* str) {
    __load_BX( &str ); // Load pointer to string into BX
    __load_CX( strlen(str) ); // Load string length into CX
    asm( "movw AX, 0x0" ); // Specify syscall type
    asm( "syscall" ); // Invoke syscall
}