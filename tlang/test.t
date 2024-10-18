#include <stdlib.t>
// #include <string.t>
#include <stdio.t>

int main() {
    int i;
    for (i = 2; i >= -3; i = i - 1) {
        print( itoa(i) );
    }
    print("\n");
    return EXIT_SUCCESS;

    // char* numStr = itoa(9812);
    // strcat(numStr, "\n");
    // print(numStr);
    // free(numStr);
    // return EXIT_SUCCESS;
}