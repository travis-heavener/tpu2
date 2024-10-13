#include <stdlib.t>
#include <string.t>
#include <stdio.t>

int main() {
    char* msgA = "Hello there, ";
    char* name = "user";
    char* msgB = "!\n";

    strcat(msgA, name);
    strcat(msgA, msgB);
    print(msgA);

    char* dest;
    strcpy(dest, name);
    print(dest);

    return EXIT_SUCCESS;
}