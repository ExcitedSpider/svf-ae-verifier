int main(void)
{
    int buffer[10];
    int end = 10;
    buffer[end] = 1;

    int far = 200;
    buffer[far] = 2;
    return 0;
}

// EXPECT: line 5 store unproved ae-out-of-bounds-if-reached
// EXPECT: line 8 store unproved ae-out-of-bounds-if-reached
// OFFSET: line 5 40 40
// OFFSET: line 8 800 800
