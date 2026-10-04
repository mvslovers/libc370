/* NAMES: collect strings in a dynamic array */
#include <stdio.h>
#include <stdlib.h>
#include <ext/array.h>

int main(void)
{
    char        **names = NULL;         /* the array starts out empty */
    unsigned    n, count;

    arrayaddf(&names, "SYS1.%s", "LINKLIB");
    arrayaddf(&names, "SYS1.%s", "MACLIB");
    arrayaddf(&names, "SYS1.%s", "PROCLIB");

    count = arraycount(&names);
    for (n = 0; n < count; n++)         /* the items are names[0] ... */
        printf("%u: %s\n", n + 1, names[n]);

    for (n = 0; n < count; n++)         /* the items are the program's */
        free(names[n]);
    arrayfree(&names);                  /* names is NULL again */
    return 0;
}
