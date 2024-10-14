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

#include <stdio.t>

/********* DYNAMIC HEAP MEMORY ALLOCATION *********/

void* malloc(const int size) {
    asm("movw AX, 0x04");   // specify syscall type
    __load_CX(size);        // load the requested size into CX
    asm("syscall");         // invoke kernel malloc function
    return __read_DX();     // return the address stored in DX
}

void* realloc(const void* addr, const int size) {
    asm("movw AX, 0x05");   // specify syscall type
    __load_BX(addr);        // load the existing allocation address into BX
    __load_CX(size);        // load the requested size into CX
    asm("syscall");         // invoke kernel malloc function
    return __read_DX();     // return the address stored in DX
}

void free(const void* addr) {
    asm("movw AX, 0x06");   // specify syscall type
    __load_BX(addr);        // load the existing allocation address into BX
    asm("syscall");         // invoke kernel malloc function
}

/********* CHAR-RELATED FUNCTIONS *********/

int isspace(const char c) {
    return c == ' ' || c == '\n' || c == '\t' || c == '\v' || c == '\f' || c == '\r';
}

int isdigit(const char c) {
    return c >= '0' && c <= '9';
}

int isxdigit(const char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

int isupper(const char c) {
    return c >= 'A' && c <= 'Z';
}

int islower(const char c) {
    return c >= 'a' && c <= 'z';
}

int isalpha(const char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int isalnum(const char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

int iscntrl(const char c) {
    return (c >= 0 && c <= 31) || c == 127;
}

int ispunct(const char c) {
    return (c >= 33 && c <= 47) || (c >= 58 && c <= 64) || (c >= 91 && c <= 96) || (c >= 123 && c <= 126);
}

int isprint(const char c) {
    return c >= 32 && c <= 126;
}

/********* TYPE CASTING FUNCTIONS *********/

// Converts a string to an integer, or 0 if the conversion failed.
int atoi(const char* str) {
    int val = 0;

    // iterate over string
    int i = str[0] == '-';
    while (str[i] != '\0') {
        if (!isdigit(str[i])) {
            return 0;
        }

        // grab value
        val = val * 10;
        val = val + str[i] - '0';
        i = i + 1;
    }

    // grab negative
    if (str[0] == '-') {
        val = val * -1;
    }

    return val;
}

// Converts an integer to a string.
const char* itoa(const int n) {
    // count places of ten
    int len = 1 + (n < 0); // add space for null byte
    int tempNum = n;
    while (tempNum = tempNum / 10) {
        len = len + 1;
    }

    // allocate string (with space for null terminator)
    char* str = malloc(sizeof(char) * len);

    // write the entire string
    int i;
    tempNum = n;
    for (i = len; i >= 0; i = i - 1) {
        if (i == 0 && n < 0) {
            str[i] = '-';
        } else if (i == len) {
            str[i] = '\0';
        } else {
            str[i] = (unsigned int)(tempNum % 10) + '0';
            tempNum = tempNum / 10;
        }

        if (i == 0) {
            return str;
        }
    }

    // return string
    return str;
}