#include <stdlib.h>

int main(void)
{
    char *data = malloc(100);
    if (data == NULL)
        return 1;
    data[0] = 'a';      /* line 8: in bounds */
    data[99] = '\0';    /* line 9: in bounds */
    free(data);
    return 0;
}

// EXPECT: line 8 store proved ae-in-bounds
// EXPECT: line 9 store proved ae-in-bounds
