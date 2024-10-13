#include <stdlib.t>
#include <string.t>
#include <stdio.t>

int main() {
    char name[] = "Travis Heavener";
    char* msg;

    if (strlen(name) > 10) {
        msg = "You have a long name\n";
    } else {
        msg = "You have a short name\n";
    }

    print(name);
    print("\n");
    print(msg);
    return EXIT_SUCCESS;
}