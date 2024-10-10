// #include <stdlib.t>
#include <string.t>
// #include <stdio.t>

// int test(int ppMat[2][3]) {
//     return *(ppMat[1]+2);
// }

// int test2(int ppMat[][3]) {
//     return ppMat[1][2];
//     // return test(ppMat);
// }

int main() {
    /************* UNTESTED *************/

    char msg[] = "Hello world!\n";
    return strlen(msg);

    // print(msg);
    // return msg[13];
    // return &msg == msg;
    // return -(int)msg;
    // return *(msg+1) == msg[1];
    // return *(msg+1);
    // return dummy(msg);
    // return strlen(msg);

    // untested
    // return test(mat);
    // return *(mat+1);
    // return &*&*&*&*&*&*mat;
    // return (mat+1) == mat[1];
    // return mat+1;
    // return *(mat[1]);
    // return *(*(mat+1)+2) == mat[1][2];
    // return mat[1][2];
    // return mat[1];
    // return *(*(mat+1)+2);
    // return *(mat+1);
    // return &mat == mat;

    /************* WORKING *************/

    // works
    // int a = 99;
    // int* pA = &a;
    // int** mat = &pA;

    // **mat = 11;
    // *pA = 11;
    // a = 11;
    // return **mat;
    // return *pA;
    // return a;
    // return *&*&*&mat;
    // return *&*&*&mat == mat;

    /******* tests 2-d arrays *******/

    // int mat[2][3] = {
    //     {1, 2, 3},
    //     {4, 5, 6}
    // };

    // assignment works
    // mat[1][2] = 0;
    // return mat[1][2];

    // works
    // return mat;
    // return mat[1];
    // return mat[0][1];
    // return &mat;
    // return *mat;
    // return **mat;
    // return *(mat+1) == mat[1];
    // return **(mat+1);
    // return **(mat+1) == mat[1][0];
    // return *(*(mat+1)+2);
    // return (*(mat+1)+2);
    // return (*(mat+1))[2];
    // return *&*mat;
    // return **&*mat;
    // return sizeof mat;
    // return sizeof mat[0];
    // return sizeof mat[0][1];
    // return mat + 1;
    // return mat[1];
    // return (*mat + 1);
    // return *(*mat + 1) == mat[0][1];
    // return mat[1][2];
    // return *(*(mat + 1) + 2);
    // return *(*(mat + 1) + 2) == mat[1][2];
    // return test(mat);
    // return test2(mat);
}