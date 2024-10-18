#ifndef __GLOBALS_HPP
#define __GLOBALS_HPP

#include <cstdint>
#include <string>
#include <sys/stat.h>

/**
 * .--------------.
 * |- Memory Map -|
 * |--------------|
 * | RESERVED     | 0x0000 - 0x07FF (2 KiB)
 * |--------------|
 * | CALLSTACK    | 0x0800 - 0x0FFF (2 KiB)
 * |--------------|
 * | .DATA        | 0x1000 - 0x17FF (2 KiB)
 * |--------------|
 * |              |
 * | .TEXT        | 0x1800 - 0x27FF (4 KiB)
 * |              |
 * |--------------|
 * |              |
 * | STACK        | 0x2800 - 0x37FF (4 KiB)
 * |              |
 * |--------------|
 * |              |
 * |              |
 * |              |
 * | HEAP         | 0x3800 - 0xFFFF (50 KiB)
 * |              |
 * |              |
 * |              |
 * '--------------'
 */

// allocate 2KiB for OS
#define RESERVED_LOWER_ADDR   0x0000
#define RESERVED_UPPER_ADDR   0x07FF

// allocate 2KiB for callstack
#define CALLSTACK_LOWER_ADDR  0x0800
#define CALLSTACK_UPPER_ADDR  0x0FFF

// allocate 2KiB for .data section
#define DATA_LOWER_ADDR       0x1000
#define DATA_UPPER_ADDR       0x17FF

// allocate 4KiB for .text section
#define TEXT_LOWER_ADDR       0x1804
#define TEXT_UPPER_ADDR       0x27FF
#define INSTRUCTION_PTR_START TEXT_LOWER_ADDR-4 // needs 4 bytes (JMP opcode, MOD byte, lower-addr, upper-addr)

// allocate 4KiB for stack
#define STACK_LOWER_ADDR      0x2800
#define STACK_UPPER_ADDR      0x37FF

// allocate 50KiB remaining for free use
#define HEAP_LOWER_ADDR       0x3800
#define HEAP_UPPER_ADDR       0xFFFF
#define HEAP_SIZE HEAP_UPPER_ADDR - HEAP_LOWER_ADDR + 1

// clock frequency for TPU
#define CLOCK_FREQ_HZ 5'000

#define T_NULL 0

/********************************************************/
/*                    MACROS & TYPES                    */
/********************************************************/

#define TAB "    "

typedef uint32_t  u32;
typedef int32_t   s32;
typedef uint16_t  u16;
typedef int16_t   s16;
typedef uint8_t   u8;
typedef int8_t    s8;

// reserved labels
#define RESERVED_LABEL_MAIN     "_main"
#define RESERVED_LABEL_MALLOC   "_malloc"
#define RESERVED_LABEL_REALLOC  "_realloc"
#define RESERVED_LABEL_FREE     "_free"

#define DATA_TYPE_STRZ ".strz" // null-terminated string

// for TCC
#define FUNC_LABEL_PREFIX       "__UF" // for "user function"
#define FUNC_END_LABEL_SUFFIX   'E' // added to the end of a function label to mark where a function ends
#define JMP_LABEL_PREFIX        "__J" // really just used for jmp instructions
#define STR_DATA_LABEL_PREFIX   "__US" // for "user string"

bool isReservedKernelFuncLabel(const std::string& label);

/********************************************************/
/*                    STRING METHODS                    */
/********************************************************/

// helper for trimming strings in place
void ltrimString(std::string& str);
void rtrimString(std::string& str);
void trimString(std::string& str) ;

// used to expand an escaped character string
char escapeChar(const std::string& str);
void escapeString(std::string& str);

/********************************************************/
/*                     I/O METHODS                      */
/********************************************************/

// modified from https://stackoverflow.com/a/6296808
bool doesFileExist(const std::string& file);

#endif