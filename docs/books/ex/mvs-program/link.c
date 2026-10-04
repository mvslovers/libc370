#include <stdio.h>
#include <mvs/link.h>

int main(void)
{
    /* PARM='HELLO' as a program started by EXEC PGM= sees it */
    static struct {
        short len;
        char  text[5];
    } parm = { 5, "HELLO" };
    void *plist[1];
    int  pgmrc = 0;
    int  abend;

    plist[0] = (void *)((unsigned)&parm | 0x80000000);

    abend = __linkt("IEFBR14", NULL, plist, &pgmrc);
    if (abend) {
        printf("IEFBR14 abended, code %06X\n", abend);
        return 8;
    }
    if (pgmrc == -1) {
        printf("IEFBR14 could not be found\n");
        return 8;
    }
    printf("IEFBR14 ended with return code %d\n", pgmrc);
    return 0;
}
