// #include <stdlib.t>
// #include <string.t>

int main() {
    // ---------- tested ----------
    // int j = -33;
    // return -j;

    // int a = 33 - 66;
    // return (a - 99 + 1) & 255;

    // int a = -2;
    // return (char)a;

    /******** CURRENTLY BROKEN ********/
    int mat[2][3] = {
        {0+1, 1+1, 2+1},
        {3+1, 4+1, 5+1}
    };

    // mat[1][2] = mat[1][1];
    *(*(mat+1)+2) = *(*(mat+1)+1);
    return mat[1][2];

    // char name[] = "Travis";
    // name[2] = 'A';
    // return name[2];

    // int name[] = "Travis";
    // name[2] = 'A';
    // return name[2];

    // int a = 0;
    // a = 'A';
    // return a;
}