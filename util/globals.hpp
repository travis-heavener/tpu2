#ifndef __GLOBALS_HPP
#define __GLOBALS_HPP

#include <cstdint>
#include <string>
#include <sys/stat.h>

/********************************************************/
/*                    MACROS & TYPES                    */
/********************************************************/

#define TAB "    "

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

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