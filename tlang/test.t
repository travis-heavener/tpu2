// #include <stdlib.t>

int main() {
    // int name[] = "Travis";
    // return strlen(name);

    int A[5] = {1, 2, 3, 4, 5};
    int* pA = &A;
    int** ppA = &pA;
    return *(ppA - 1)[0];
    // return (*ppA)[0];
    // return (*ppA)[0];
    // return pA[0];

    // int mat[2][3] = {
    //     {1, 2, 3+9+1},
    //     {4, 5, 6}
    // };
    // return 19 + ~(((((mat[0][2])))));

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