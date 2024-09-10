int main(int a) {
    int i; // initialized to 0
    int j; // initialized to 0
    int k; // initialized to 0

    for (i = 0; i < 10; i = i + 2) {
        for (j = 1; j <= 3; j = j + 1) {
            k = k + i * j;

            // force break out of the loop
            if (k > 30) {
                k = 9999;
                j = 4;
                i = 10;
            }
        }
    }

    // quick lil cheat bc result is in AX and that's currently printed to STDOUT
    k + 0;
}