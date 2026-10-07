/* WHERE: tell how the program was started, and show its arguments */
#include <stdio.h>
#include <mvs/crt.h>

int main(int argc, char **argv)
{
    CLIBPPA *ppa = __ppaget();
    CLIBGRT *grt = __grtget();
    int      i;

    if (ppa->ppaflag & PPAFLAG_TSOFG)
        printf("TSO foreground session\n");
    else if (ppa->ppaflag & PPAFLAG_TSOBG)
        printf("TSO in a batch job\n");
    else
        printf("batch job step\n");

    if (ppa->ppacppl)
        printf("started as a command processor\n");
    else if (grt->grtflag1 & GRTFLAG1_TSO)
        printf("TSO-style parameter, but no CPPL\n");
    else
        printf("started with a PARM string\n");

    for (i = 0; i < argc; i++)
        printf("argv[%d] = \"%s\"\n", i, argv[i]);
    return 0;
}
