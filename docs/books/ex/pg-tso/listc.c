/* LISTC: run the TSO command LISTCAT for a level given as argument */
#include <stdio.h>
#include <mvs/crt.h>
#include <mvs/tso.h>

int main(int argc, char **argv)
{
    CLIBPPA *ppa = __ppaget();
    int      rc;

    if (argc < 2) {
        fprintf(stderr, "usage: LISTC level\n");
        return 4;
    }

    /* tsocmd() needs the CPPL of a command processor; without one it
       returns 8 and writes a message to the operator.  Test first. */
    if (!ppa || !ppa->ppacppl) {
        fprintf(stderr, "LISTC must be run as a TSO command\n");
        return 8;
    }

    rc = tsocmdf("LISTCAT", "LEVEL(%s)", argv[1]);
    if (rc < 0)
        fprintf(stderr, "LISTCAT could not be called\n");
    else
        printf("LISTCAT ended with return code %d\n", rc);
    return rc < 0 ? 8 : rc;
}
