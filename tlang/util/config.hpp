#ifndef __CONFIG_HPP
#define __CONFIG_HPP

// constants for the entire program
#define FUNC_MAIN_LABEL "main"
#define FUNC_MAIN_NAME "main"
#define FUNC_LABEL_PREFIX "__UF" // for "user function"
#define FUNC_END_LABEL_SUFFIX "E" // added to the end of a function label to mark where a function ends
#define JMP_LABEL_PREFIX "__J" // really just used for jmp instructions
#define TAB "    "

inline bool DELETE_UNUSED_VARIABLES;
inline bool DELETE_UNUSED_FUNCTIONS;

#endif