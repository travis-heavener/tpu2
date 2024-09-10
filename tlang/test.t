int main(int a) {
    int i; // initialized to 0
    int j; // initialized to 0
    int k; // initialized to 0

    while (i < 15) {
        while (j < i) {
            k = k + j;
            j = j + 1;
        }
        k = k + i;
        i = i + 1;

        if (k % 2 == 3) {
            return k;
        }
    }

    return 10;
    k = 11;

    // quick lil cheat bc result is in AX and that's currently printed to STDOUT
    k + 0;
}