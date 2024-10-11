/**
 * Extension of the "T Standard Library" for IO-related support.
 * 
 * @author: Travis Heavener
 */

#include <string.t>

void print(char str[]) {
    int len = strlen(str);  // Predetermine length to prevent overwriting BX or CX
    __load_BX( str );       // Load pointer to string into BX
    __load_CX( len );       // Load string length into CX
    asm( "movw AX, 0x0" );  // Specify syscall type
    asm( "syscall" );       // Invoke syscall
}