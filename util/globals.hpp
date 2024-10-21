#ifndef __GLOBALS_HPP
#define __GLOBALS_HPP

#include <cstdint>
#include <stdexcept>
#include <string>
#include <sys/stat.h>

/**
 * .--------------.
 * |- Memory Map -|
 * |--------------|
 * |              |
 * | RESERVED     | 0x0000 - 0x0FFF (4 KiB)
 * |              |
 * |--------------|
 * | CALLSTACK    | 0x1000 - 0x17FF (2 KiB)
 * |--------------|
 * |              |
 * | STACK        | 0x1800 - 0x27FF (4 KiB)
 * |              |
 * |--------------|
 * |              |
 * |              |
 * |              |
 * | HEAP         | 0x2800 - 0xFFFF (54 KiB)
 * |              |
 * |              |
 * |              |
 * '--------------'
 */

// reserve 4 KiB for OS
#define RESERVED_LOWER_ADDR   0x0000
#define RESERVED_UPPER_ADDR   0x0FFF
#define MAX_OS_IMAGE_SIZE     0x07FF // 2 KiB for the OS & kernel
#define PROGRAM_INDEX         RESERVED_UPPER_ADDR - 1
#define INSTRUCTION_PTR_START RESERVED_LOWER_ADDR

// allocate 2KiB for callstack
#define CALLSTACK_LOWER_ADDR  0x1000
#define CALLSTACK_UPPER_ADDR  0x17FF

// allocate 4KiB for stack
#define STACK_LOWER_ADDR      0x1800
#define STACK_UPPER_ADDR      0x27FF

// allocate 50KiB remaining for free use
#define HEAP_LOWER_ADDR       0x2800
#define HEAP_UPPER_ADDR       0xFFFF
#define HEAP_SIZE HEAP_UPPER_ADDR - HEAP_LOWER_ADDR + 1

// clock frequency for TPU
#define CLOCK_FREQ_HZ   5'000
#define CYCLE_TIME      1.0d / CLOCK_FREQ_HZ

// addressing mode defines
#define ADDRESS_MODE_ABSOLUTE 0
#define ADDRESS_MODE_RELATIVE 1

#define T_NULL 0

// CPI for various instructions
#define DISK_READ_TIME      1e-5 // response time to read 1 byte from disk (100,000 bytes/sec)
#define DISK_WRITE_TIME     2e-6 // response time to write 1 byte to disk (20,000 bytes/sec)
#define CPI_DISK_READ       (u16)ceil((double)DISK_READ_TIME * CLOCK_FREQ_HZ)
#define CPI_DISK_WRITE      (u16)ceil((double)DISK_WRITE_TIME * CLOCK_FREQ_HZ)

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
/*                     TPU METHODS                      */
/********************************************************/

// flag macros
// ref: https://www.geeksforgeeks.org/flag-register-8086-microprocessor/?ref=lbp
#define CARRY 0     // set if a carry/borrow bit is used during an arithmetic operation
#define PARITY 2    // set if the result of arithmetic or logical operation has odd parity
#define ZERO 6      // set if the result of arithmetic or logical operation is zero
#define SIGN 7      // set if the result of arithmetic or logical operation is negative
#define OVERFLOW 11 // set if the result of arithmetic operation overflows/underflows

// instruction set opcodes
enum OPCode {
    NOP         = 0x00,
    HLT         = 0x01,
    SYSCALL     = 0x02,
    CALL        = 0x03,
    RET         = 0x04,
    JMP         = 0x05,
    MOV         = 0x06,
    MOVW        = 0x07,
    PUSH        = 0x08,
    POP         = 0x09,
    POPW        = 0x0A,
    ADD         = 0x14,
    SUB         = 0x15,
    MUL         = 0x16,
    DIV         = 0x17,
    CMP         = 0x18,
    BUF         = 0x1F,
    AND         = 0x20,
    OR          = 0x21,
    XOR         = 0x22,
    NOT         = 0x23,
    SHL         = 0x24,
    SHR         = 0x25
};

// register codes
enum Register {
    AX      = 0x00,     AL      = 0x01,     AH      = 0x02,
    BX      = 0x03,     BL      = 0x04,     BH      = 0x05,
    CX      = 0x06,     CL      = 0x07,     CH      = 0x08,
    DX      = 0x09,     DL      = 0x0A,     DH      = 0x0B,
    SP      = 0x0C,     BP      = 0x0D,
    SI      = 0x0E,     DI      = 0x0F,     IP      = 0x10,
    CP      = 0x11,     ES      = 0x12,     FLAGS   = 0x13
};

// syscall codes
enum Syscall {
    STDOUT      = 0x00,     STDERR      = 0x01,     STDIN       = 0x02,
    EXIT_STATUS = 0x03,     MALLOC      = 0x04,     REALLOC     = 0x05,
    FREE        = 0x06,     ADDR_MODE   = 0x07
};

constexpr Register getRegister16FromCode(unsigned short code) {
    switch (code) {
        case AX: return AX;     case BX: return BX;
        case CX: return CX;     case DX: return DX;
        case SP: return SP;     case BP: return BP;
        case SI: return SI;     case DI: return DI;
        case IP: return IP;     case CP: return CP;
        case FLAGS: return FLAGS;
        default: throw std::invalid_argument("OPCode does not belong to a 16-bit register: " + code);
    }
}

constexpr Register getRegister8FromCode(unsigned short code) {
    switch (code) {
        case AL: return AL;  case AH: return AH;
        case BL: return BL;  case BH: return BH;
        case CL: return CL;  case CH: return CH;
        case DL: return DL;  case DH: return DH;
        default: throw std::invalid_argument("OPCode does not belong to an 8-bit register: " + code);
    }
}

Register getRegisterFromString(const std::string&);

constexpr bool isRegister8Bit(Register reg) {
    return reg == Register::AL || reg == Register::AH || reg == Register::BL || reg == Register::BH || reg == Register::CL || reg == Register::CH || reg == Register::DL || reg == Register::DH;
}

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