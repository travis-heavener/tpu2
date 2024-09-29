#include <stdlib.t>
#include <string.t>

int main() {
    int mat[2][3] = {
        {1, 2, 3+9+1},
        {4, 5, 6}
    };

    // return sizeof mat;
    return sizeof (**mat);

    // return mat['\0']['\0'];
    // return (*(mat+1))[2];
    // return *(*(mat+1)+2);
    // return *(mat[1] + 2);
    // return **(mat+1);
    // return *(mat+1);
    // return mat[1][2];
    // return *mat[1];
    // return mat[1];
    // return mat;
    // return mat[0][0+1];

    // expected 50
    // int x = (4 + 6 * (3 - 1) / 2) * (8 - (3 + 1)) + 12 / 3 - (5 % 3) * 2 + (2 * 4 + (8 / 4));
    
    // expected 41
    // int x = (7 + 2 * (5 - 3) / 1) * (9 - (4 + 2)) + 15 / 3 - (8 % 5) * 3 + (3 * 3 + (12 / 4));

    // expected 84
    // int x = (3 + 4)  * (3 + 9);

    // expected 65535
    // int x = ~(int)false;
    // return x;
}