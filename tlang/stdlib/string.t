/**
 * Extension of the "T Standard Library" for string support.
 */

int strlen(char str[]) {
    int i = 0;

    while (str[i] != '\0') {
        i = i + 1;
    }

    return i;
}