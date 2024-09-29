/**
 * The "Standard Library" for the T Language.
 */

#define NULL 0

int strlen(char* str) {
    int i = 0;

    while (str[i] != '\0') {
        i = i + 1;
    }

    return i;
}