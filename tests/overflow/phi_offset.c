#include <string.h>

int flag(void);

int main(void)
{
    char *data;
    char buffer[100];
    if (flag())
        data = buffer;
    else
        data = buffer;
    data[0] = 'a';                /* line 13: in bounds */
    memset(data, 'C', 99);        /* line 14: in bounds */
    data[99] = '\0';              /* line 15: in bounds */
    return 0;
}

// EXPECT: line 13 store proved ae-in-bounds
// EXPECT: line 14 call-write proved ae-in-bounds
// EXPECT: line 15 store proved ae-in-bounds
