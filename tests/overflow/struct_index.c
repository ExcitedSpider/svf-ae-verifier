struct padded { char c; int x; };

int main(void)
{
    struct padded object;
    struct padded array[10];
    int i = 3;
    object.x = 1;
    array[i].x = 2;
    return 0;
}

// EXPECT: line 8 store proved ae-in-bounds
// EXPECT: line 9 store unproved ae-bounds-unknown
// OFFSET: line 8 4 4
