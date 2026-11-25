int flag(void);
char *unknown(void);

int main(void)
{
    char a[10], b[10];
    char *p = flag() ? a : b;
    *p = 1;
    char *q = flag() ? a : unknown();
    *q = 2;
    char *r = flag() ? a : a + 10;
    *r = 3;
    char *s = a;
    while (flag()) s++;
    *s = 4;
    return 0;
}

// EXPECT: line 8 store proved ae-in-bounds
// EXPECT: line 10 store unproved ae-bounds-unknown
// EXPECT: line 12 store unproved ae-bounds-unknown
// EXPECT: line 15 store unproved ae-bounds-unknown
