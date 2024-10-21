/**
 * Extension of the "T Standard Library" for IO-related support.
 * 
 * @author: Travis Heavener
 */

#include <string.t>

#define MAX_READ_LEN 255

void print(const char* str) {
    int len = strlen(str);  // Predetermine length to prevent overwriting BX or CX
    __load_BX( str );       // Load pointer to string into BX
    __load_CX( len );       // Load string length into CX
    asm( "movw AX, 0x0" );  // Specify syscall type
    asm( "syscall" );       // Invoke syscall
}

void readline(char* dest) {
    // Reads the entire next line from STDIN (up to 255 characters)
    __load_BX( dest );          // Load destination ptr to BX
    __load_CX( MAX_READ_LEN );  // Load read size to CL
    asm( "movw AX, 0x02" );     // Specify syscall type
    asm( "syscall" );           // Invoke syscall

    // Place null terminator after first newline
    int i;
    for (i = 0; i < MAX_READ_LEN; i = i + 1) {
        if (dest[i] == '\n') {
            dest[i] = '\0';
            return;
        }
    }
}