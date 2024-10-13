# TPU2 - Travis Heavener
The Terrible Processing Unit, a 16-bit CPU using a non-segmented 64KiB of memory and is loosely based off of the 8086 processor.

## Description

This project mostly just serves as a fun proof-of-concept for something that I was mildly interested in and had the time to do, like 90% of my other projects. It's not intended to have perfect accuracy in mirroring the actual function of an x86 processor nor is it perfectly fine-tuned to be as efficient as possible.

There are parts of this project (such as the assembler) that I would like to refurbish and improve upon in the near future, but I wanted to at least make the assembler so that it was much easier to test everything instead of manually hard-coding programs to memory.

The full instruction set: [instruction_set.txt](references/instruction_set.txt)

The full set of syscalls: [syscall_set.txt](references/syscall_set.txt)

## The T Programming Language

The T Programming Language is largely based on C syntax. In fact, most (if not all) T programs can be ported to C without changing anything\*.

TCC (TPU Crappy Compiler) is the compiler used to compile T files to TPU assembly files, linking any included T files needed as a C compiler would (ex. GCC).

\* A few changes such as I/O functions and header conventions vary slightly from that of C, as does the TCC preprocessor.

## How to use

***This is designed using Linux (Ubuntu 22).***

I have NOT tested this for Windows, although `<conio.h>` *should* work in place of ncurses since I wanted the emulated input stream to wait for character input (hence why I used ncurses on Ubuntu).

Install ncurses (Linux only): `sudo apt-get install libncurses-dev`

To compile (Linux only): `make base`

To compile the T-language compiler (Linux only); `make tcc`

To compile the TPU Post-Processor: `make postproc`

To compile all of the above: `make all`

## Disclaimer

1) ***THIS IS A WORK IN PROGRESS. THERE ARE ~~PROBABLY~~ POSSIBLY BUGS.***
2) Making syscalls to STDIN can be buggy given that this is an emulation and there are only so many ways to get keyboard input. On Linux, the ncurses library is used for this purpose and will very likely show that it is leaking memory--IT'S WORKING AS INTENDED. It's an external library so it has to manage its own memory, or something along those lines (I couldn't find a clear answer). TL;DR I'm not leaking memory, I promise :).