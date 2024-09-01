# TPU2 - Travis Heavener
The Terrible Processing Unit, a 16-bit CPU using a non-segmented 64KiB of memory and is loosely based off of the 8086 processor.

### A few notes to self

1) ***THIS IS A WORK IN PROGRESS. THERE ARE PROBABLY BUGS.***
2) Making syscalls to STDIN can be buggy given that this is an emulation and there are only so many ways to get keyboard input. On Linux, the ncurses library is used for this purpose and will very likely show that it is leaking memory--IT'S WORKING AS INTENDED. It's an external library so it has to manage its own memory, or something along those lines (I couldn't find a clear answer). TL;DR I'm not leaking memory, I promise :).