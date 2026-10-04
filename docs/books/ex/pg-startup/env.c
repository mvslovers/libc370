#include <stdio.h>
#include <stdlib.h>

/* Take a setting from the SYSENV DD, with a default. */
int main(void)
{
    const char *limit = getenv("LIMIT");

    if (limit == NULL)
        limit = "100";
    printf("limit is %s\n", limit);
    return 0;
}
