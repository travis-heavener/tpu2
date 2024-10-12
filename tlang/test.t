#include <stdlib.t>
#include <string.t>
#include <stdio.t>

int main() {
    char name[] = "What's up danger\n";

    if (strlen(name) > 10) {
        char msg[] = "Long message incoming...\n";
        print(msg);
    } else {
        char msg[] = "Short message incoming...\n";
        print(msg);
    }

    print(name);
    return EXIT_SUCCESS;
}