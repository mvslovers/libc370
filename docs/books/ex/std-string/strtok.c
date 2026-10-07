#include <stdio.h>
#include <string.h>

int main(void)
{
    char dsn[] = "SYS1.MACLIB,SYS1.AMODGEN,,SYS2.MACLIB";
    char *tok;

    /* an empty field between two commas yields no token */
    for (tok = strtok(dsn, ","); tok != NULL; tok = strtok(NULL, ","))
        printf("%-8.8s... (%lu characters)\n", tok,
               (unsigned long)strlen(tok));
    return 0;
}
