#include <stdlib.t>
#include <string.t>
#include <stdio.t>

void doubleMyVar(int* pMyVar) {
    *pMyVar = *pMyVar * 2;
}

int main() {
    int myVar = 10;
    doubleMyVar(&myVar);
    return myVar;
}