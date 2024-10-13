/**
 * Extension of the "T Standard Library" for string support.
 * 
 * @author: Travis Heavener
 */

int strlen(const char* str) {
    int i = 0;

    while (str[i] != '\0') {
        i = i + 1;
    }

    return i;
}

char* strcpy(char* dest, const char* src) {
    int len = strlen(src);

    // copy the string
    int i;
    for (i = 0; i <= len; i = i + 1) {
        dest[i] = src[i];
    }

    // return destination
    return dest;
}

char* strcat(char* dest, const char* src) {
    int destLen = strlen(dest);
    int srcLen = strlen(src);

    // copy the string
    int i;
    for (i = 0; i <= srcLen; i = i + 1) {
        dest[destLen + i] = src[i];
    }

    // return destination
    return dest;
}