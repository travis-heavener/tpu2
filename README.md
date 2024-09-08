# TPU2 - Travis Heavener
The Terrible Processing Unit, a 16-bit CPU using a non-segmented 64KiB of memory and is loosely based off of the 8086 processor.

## Description

This project mostly just serves as a fun proof-of-concept for something that I was mildly interested in and had the time to do, like 90% of my other projects. It's not intended to have perfect accuracy in mirroring the actual function of an x86 processor nor is it perfectly fine-tuned to be as efficient as possible.

There are parts of this project (such as the assembler) that I would like to refurbish and improve upon in the near future, but I wanted to at least make the assembler so that it was much easier to test everything instead of manually hard-coding programs to memory.

The error handling in this project is also atrocious, I could have done better as seen in some other projects like my WIP assembled programming language, Deuterium. I will add again that I was ***not*** going for perfection but instead a fast implementation that got the job done well.

TL;DR: I'll greatly improve and extend this project when I get the chance. If this seems half-assed, just know that I have been on a time crunch for this project and have only had <48 hours to work on this.

The full instruction set: [instruction_set.txt](references/instruction_set.txt)

The full set of syscalls: [syscall_set.txt](references/syscall_set.txt)

## How to use

***This is designed using Linux (Ubuntu 22.04.3 LTS via WSL).***

I have NOT tested this for Windows, although `<conio.h>` *should* work in place of ncurses since I wanted the emulated input stream to wait for character input (hence why I used ncurses on Ubuntu).

Install ncurses (Linux only):
`sudo apt-get install libncurses-dev`

To compile (Linux only):
`g++ *.cpp util/*.cpp -o ./bin/main.o -Wall -Wextra -g -lncurses`

## Disclaimer

1) ***THIS IS A WORK IN PROGRESS. THERE ARE ~~PROBABLY~~ POSSIBLY BUGS.***
2) Making syscalls to STDIN can be buggy given that this is an emulation and there are only so many ways to get keyboard input. On Linux, the ncurses library is used for this purpose and will very likely show that it is leaking memory--IT'S WORKING AS INTENDED. It's an external library so it has to manage its own memory, or something along those lines (I couldn't find a clear answer). TL;DR I'm not leaking memory, I promise :).
3) My terminology is likely flawed in various parts of the documentation (ex. the instruction set). I'm still going through the motions of learning all these really, *really* low level bits about how CPU architectures work and how x86 assembly works. I'll probably improve this in a few weeks since I coincidentally happen to be starting a MIPS course (similar enough in the ways that count).