/*
 * tstlsd308.c - libc370 #308 on MVS: __listds() returns the same list as
 * before for real LISTCAT output.
 *
 * The fix (src/mvs/dslist/@@listds.c) takes an entry keyword only at the left
 * margin of a LISTCAT line, so that an indented association line such as
 * "CLUSTER--name" does not start an entry.  The host test (test/host/
 * tstlstds.c) pins the parse on fixtures; this run answers whether real
 * output, as __listc() reads it back from its OUTFILE, has its entry lines
 * there.  If it did not, the fixed library would list nothing.
 *
 * It prints every record as "dsn volser" for each level and option.  Built
 * twice from this source: TSTL308 against this tree's libc.a, TSTL308R
 * against the installed sysroot libc.a (before #308).  The two SYSPRINTs are
 * compared on the host; on a catalog without a VOLSER-less entry they must
 * be identical.
 *
 * PARM='LEVEL1 LEVEL2 ...'  (default IBMUSER SYS1 SYS2)
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstlsd308.c -o TSTL308 -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude \
 *                test/mvs/tstlsd308.c -o TSTL308R -flinker-output=iebcopy
 *          ld370 --pack TSTL308=TSTL308.iebcopy TSTL308R=TSTL308R.iebcopy \
 *                -o tstlsd308 -xmit --dsn IBMUSER.LIBC370.L308SCR
 * Install: jcl/recvl308.jcl.   Run: jcl/tstlsd308.jcl.
 *
 * mvsdev JOB01340, 2026-10-04 (RECEIVE JOB01339): IBMUSER 35, SYS1 96,
 * SYS2 11 records with either option, identical in both libraries - except
 * LEVEL('SYS1') VOLUME: the previous library listed SYS1.PAGECSA,
 * SYS1.STGINDEX and SYS1.VSAM.MASTER.CATALOG on the volume of the data set
 * after each (they have no VOLUMES block, JOB01341) and lost SYS1.PARMLIB,
 * SYS1.SVCLIB and SYS1.VTAMLIB.  The fixed one lists those three.
 *
 * RC: 0 = every level answered with records, 8 = one did not.
 */
#include <stdio.h>
#include <string.h>
#include "ext/array.h"
#include "mvs/dslist.h"

static int bad = 0;

static void one(const char *level, const char *option)
{
    DSLIST   **list = __listds(level, option, NULL);
    unsigned   n    = list ? arraycount(&list) : 0;
    unsigned   i;

    printf("LEVEL('%s') %s: %u records\n", level, option, n);
    if (!n) bad++;
    for (i = 0; i < n; i++) {
        printf("  %-44s %s\n", list[i]->dsn, list[i]->volser);
    }
    if (list) __freeds(&list);
}

int main(int argc, char **argv)
{
    static char *deflt[] = { "IBMUSER", "SYS1", "SYS2" };
    char        **lv = deflt;
    int           n  = 3;
    int           i;

    if (argc > 1) { lv = argv + 1; n = argc - 1; }

    for (i = 0; i < n; i++) {
        one(lv[i], "NONVSAM VOLUME");
        one(lv[i], "VOLUME");
    }
    printf("\nTSTL308 %s\n", bad ? "FAILED" : "DONE");
    return bad ? 8 : 0;
}
