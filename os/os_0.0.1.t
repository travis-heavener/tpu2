#include <stdlib.t>
#include <string.t>
#include <stdio.t>

int main() {
    // begin boot
    print("Image loaded.\n");

    // do shit
    char* line = malloc(MAX_READ_LEN);
    readline(line);
    strcat(line, "\n");

    print(line);

    // begin shutdown process
    print("Shutting down...\n");

    // free memory
    free( line );
    return EXIT_SUCCESS;
}