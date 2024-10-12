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

    int a = -1;
    return (unsigned int)a;

    // return sizeof (unsigned int)&name;
    // return sizeof (bool)&name;
}