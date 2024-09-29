#include <stdlib.t>

int main() {
    // int name[] = "Travis";
    // return strlen(name);

    // int mat[2][3] = {
    //     {1, 2, 3+9+1},
    //     {4, 5, 6}
    // };
    // return 19 + ~(((((mat[0][2])))));

    // int mat[2][3] = {
    //     {1, 2, 3+9+1},
    //     {4, 5, 6}
    // };

    // works
    // return mat[1][2];
    // return mat[1];
    // return *mat[1];
    // return *(*(mat+1)+2);
    // return mat[1][1];
    // return *(mat+1) + 1;
    // return (*(mat+1))+1;
    // return *(*(mat+1) + 2);
    // return (*(mat+6));

    // int x = 18;
    // int* pX = &x;
    // int** ppX = &pX;
    // return &*&*&ppX;
    // return &*ppX == ppX;
    // return **&*&ppX;
    // return &ppX;
    // return *&*&*&ppX;
    // return &*&ppX == &ppX;
    // return ppX;
    // return *&*&*&*ppX;
    // return &*&ppX;

    // works backwards and should only add the internal size instead of 2
    // return *(mat+6)[2];

    char A = 'T';
    char B = 'r';
    char C = 'a';
    char D = 'v';
    char E = 'i';
    char F = 's';
    char G = '\0';
    char* pA = &A;
    // return pA[0];
    // return *(int*)pA;
    int c = 4098;
    return *(int*)4098;

    // int j = 12;
    // int* c = &j;
    // return c;

    // int name[] = "Travis";
    // return name;
}