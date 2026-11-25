#include <string.h>
#include <wchar.h>

int main(void)
{
    char four[4], three[3], dest[12];
    wchar_t wide[4];
    strcpy(four, "abc");
    strcpy(three, "abc");
    strncpy(four, "abc", sizeof four);
    strncpy(four, "abc", sizeof four + 1);
    dest[0] = 'a';
    dest[1] = 0;
    strcat(dest, "bc");
    strncat(dest, "de", 1);
    wcscpy(wide, L"abc");
    wcsncpy(wide, L"abc", 4);
    return 0;
}

// The four string literals contribute 14 SVF global initialization stores without debug locations.
// EXPECT: line 0 store unproved ae-bounds-unknown
// EXPECT: line 0 store unproved ae-bounds-unknown
// EXPECT: line 0 store unproved ae-bounds-unknown
// EXPECT: line 0 store unproved ae-bounds-unknown
// EXPECT: line 0 store unproved ae-bounds-unknown
// EXPECT: line 0 store unproved ae-bounds-unknown
// EXPECT: line 0 store unproved ae-bounds-unknown
// EXPECT: line 0 store unproved ae-bounds-unknown
// EXPECT: line 0 store unproved ae-bounds-unknown
// EXPECT: line 0 store unproved ae-bounds-unknown
// EXPECT: line 0 store unproved ae-bounds-unknown
// EXPECT: line 0 store unproved ae-bounds-unknown
// EXPECT: line 0 store unproved ae-bounds-unknown
// EXPECT: line 0 store unproved ae-bounds-unknown
// EXPECT: line 8 call-write unproved ae-length-unknown
// EXPECT: line 8 call-read unproved ae-length-unknown
// EXPECT: line 9 call-write unproved ae-length-unknown
// EXPECT: line 9 call-read unproved ae-length-unknown
// EXPECT: line 10 call-write proved ae-in-bounds
// EXPECT: line 10 call-read proved ae-in-bounds
// EXPECT: line 11 call-write unproved ae-out-of-bounds-if-reached
// EXPECT: line 11 call-read unproved ae-bounds-unknown
// EXPECT: line 12 store proved ae-in-bounds
// EXPECT: line 13 store proved ae-in-bounds
// EXPECT: line 14 call-write unproved ae-length-unknown
// EXPECT: line 14 call-read unproved ae-length-unknown
// EXPECT: line 15 call-write unproved ae-length-unknown
// EXPECT: line 15 call-read unproved ae-length-unknown
// EXPECT: line 16 call-write unproved ae-length-unknown
// EXPECT: line 16 call-read unproved ae-length-unknown
// SVF's pinned ExtAPI has no wcsncpy annotation, so line 17 has no site.
