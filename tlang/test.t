// #include <stdlib.t>

int main() {
    // int name[] = "Travis";
    // return strlen(name);

    // int mat[2][3] = {
    //     {1, 2, 3+9+1},
    //     {4, 5, 6}
    // };
    // return 19 + ~(((((mat[0][2])))));

    int mat[2][3] = {
        {1, 2, 3+9+1},
        {4, 5, 6}
    };

    // works
    // return mat[1][2];
    // return mat[1];

    return **(mat+6);
    // return *mat[1];

    // return *(*(mat+1) + 2);
    // return (*(mat+6));

    // int x = 18;
    // int* pX = &x;
    // int** ppX = &pX;
    // return &*&*&*&ppX;
    // return &*ppX;
    // return **&*&ppX;
    // return &ppX;
    // return *&*&*&ppX;
    // return &*&ppX == &ppX;
    // return ppX;
    // return *&*&*&*ppX;
    // return &*&ppX;

    // works backwards and should only add the internal size instead of 2
    // return *(mat+6)[2];

    // int a = 1;
    // int b = 2;
    // int c = 3+9+1;
    // int d = 4;
    // int e = 5;
    // int f = 6;

    // int** ppMat = &a;
    // return (ppMat + 2);

    // int A = 'T';
    // int B = 'r';
    // int C = 'a';
    // int D = 'v';
    // int E = 'i';
    // int F = 's';
    // int G = '\0';
    // int* pA = &A;
    // return pA[0];

    // int j = 12;
    // int* c = &j;
    // return c;

    // int name[] = "Travis";
    // return name;
}