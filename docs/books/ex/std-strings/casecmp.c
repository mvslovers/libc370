#include <stdio.h>
#include <strings.h>

int main(void)
{
    const char *answer = "Yes";

    if (strcasecmp(answer, "YES") == 0)
        printf("confirmed\n");
    if (strncasecmp("SYSPRINT", "sysp", 4) == 0)
        printf("prefix matches\n");
    return 0;
}
