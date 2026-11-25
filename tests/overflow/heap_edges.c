#include <stdlib.h>

int choice(void);
unsigned long unknown(void);

int main(void)
{
    unsigned long n = choice() ? 10 : 20;
    char *a = malloc(n);
    if (!a) return 1;
    a[5] = 1;
    a[15] = 1;
    int *b = calloc(10, sizeof(int));
    if (!b) return 1;
    b[9] = 1;
    char *c = malloc(unknown());
    if (!c) return 1;
    c[0] = 1;
    char *d = malloc(10);
    if (!d) return 1;
    d[10] = 1;
    int vla[n];
    vla[0] = 1;
    return vla[0];
}

// EXPECT: line 11 store proved ae-in-bounds
// EXPECT: line 12 store unproved ae-bounds-unknown
// EXPECT: line 15 store proved ae-in-bounds
// EXPECT: line 18 store unproved ae-bounds-unknown
// EXPECT: line 21 store unproved ae-out-of-bounds-if-reached
// EXPECT: line 23 store unproved ae-bounds-unknown
// EXPECT: line 24 load unproved ae-bounds-unknown
