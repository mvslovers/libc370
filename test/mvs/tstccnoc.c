/*
 * tstccnoc.c - libc370 #507: the C side of test/mvs/tstccnoc.asm, an
 * assembler main program with no C program on the task.  See there for
 * the checks and the build.
 */
#include <stdio.h>
#include <stdlib.h>
#include "mvs/crt.h"

int cstat(int *count, void **ppa) asm("CSTAT");
int catex(void) asm("CATEX");
int cexit(void) asm("CEXIT");

static int calls;

/* one more call; the PPA of the environment it ran in */
int cstat(int *count, void **ppa)
{
    *count = ++calls;
    *ppa = __ppaget();
    printf("TSTCCNOC call %d, PPA %08X\n", calls, (unsigned)*ppa);
    return 0;
}

static void ran(void)
{
    printf("TSTCCNOC atexit ran\n");
}

int catex(void)
{
    return atexit(ran) ? 8 : 0;
}

int cexit(void)
{
    exit(5);
    return 0;
}
