/**
 * The "Standard Library" for the T Language.
 */

int strlen(char str[30]) {
    int i = 0;
    int len = 0;

    for (i = 0; str[i] != '\0'; i = i + 1) {
        len = len + 1;
    }

    return len;
}