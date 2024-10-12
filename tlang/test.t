#include <stdlib.t>
#include <string.t>
#include <stdio.t>

const int* dummy() {
    return 4098;
}

int main() {
    const unsigned int MY_CONST = 12;
    *(int*)dummy() = 9;
    return (const signed char) MY_CONST;
    // char* pS = 4098;
    // char text[] = &pS;
    // char name[] = "What's up danger\n";

    // if (strlen(name) > 10) {
    //     char msg[] = "Long message incoming...\n";
    //     print(msg);
    // } else {
    //     char msg[] = "Short message incoming...\n";
    //     print(msg);
    // }

    // print(name);
    // return EXIT_SUCCESS;
}