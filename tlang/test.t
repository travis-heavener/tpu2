void dummy() {
    // do nothing
}

int test(int a[4][2], int size) {
    return a[size-1][0] * 3;
}

// int test(int a[1][2]) {
//     return 63 + a[0][1];
// }

int main() {
    int dum[][4][2] = {
        {{0,1}, {2,3}, {4,5}, {6,7}},
        {{8,9}, {10,11}, {12,13}, {14,15}},
        {{16,17}, {18,19}, {20,21}, {22,23}}
    };
    int f[4][2] = dum[2];
    int j[4][2] = dum[2];
    int c = 90;
    int asdf[] = {1, 2, 0, 1};

    return test(f, 4);

    // char a = 23;
    // return a;
}