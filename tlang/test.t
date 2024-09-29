#include <stdlib.t>
#include <string.t>

int main() {
    // ---------- tested ----------
    // int j = -33;
    // return -j;

    // int a = 33 - 66;
    // return (a - 99 + 1) & 255;

    // int a = -2;
    // return (char)a;

    unsigned int a = -2;
    return a == 65534;
}