#include <stdlib.t>

int****** main() {
    // int c = (char*)0;
    int x = 165;
    int* pX = &*(&x+1);
    int** ppX = &pX;
    pX = pX + 2;
    // int f = *(char*)0;
    // int x = 3 + -(123);
    // // int* a[3];
    // return c;
    // return 0;
    return **ppX;
}