/**
 * The "Standard Library" for the T Language.
 */

int strlen(char* str) {
    // caused a segfault vvvv
    // return *((char*)4098);
    // caused a segfault ^^^^
    return str[0];

    // int i;
    // int len = 0;

    // for (i = 0; str[i] != '\0'; i = i + 1) {
    //     len = len + 1;
    // }

    // return len;
}