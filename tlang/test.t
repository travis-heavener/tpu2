#include <stdlib.t>

int main() {
    char name[] = "Travis";
    return strlen(name);

    // int mat[2][3] = {
    //     {1, 2, 3+9+1},
    //     {4, 5, 6}
    // };
    // return 19 + ~(((((mat[0][2])))));

    int mat[2][3] = {
        {1, 2, 3+9+1},
        {4, 5, 6}
    };

    return *(*(mat+1)+2);
}