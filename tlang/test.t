#include <stdlib.t>

int main() {
    // char name[] = "Travis";
    // return &name[10];

    // int x = 165;
    // int* pX = &x;
    // return &pX;

    char x[] = "Travis";

    return sizeof &x;
}