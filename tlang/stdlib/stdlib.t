/**
 * The "Standard Library" for the T Language.
 */

#define NULL 0

int strlen(int str[]) {
    int i;

    while (str[i] != '\0') {
        i = i + 1;
    }

    return i;
}