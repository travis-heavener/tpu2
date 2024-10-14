#include <stdlib.t>
#include <string.t>
#include <stdio.t>

int main() {
    char* numStr = itoa(9812);
    strcat(numStr, "\n");
    print(numStr);
    free(numStr);
    return EXIT_SUCCESS;
}