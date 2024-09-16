#define dummy "Hi"
#define whyWouldYouEvenUseThis 1
#define iDontKnow 6

#include "test2.t"

int main() {
    char myName[] = dummy;
    int x = whyWouldYouEvenUseThis << iDontKnow;
    return square(x);
}