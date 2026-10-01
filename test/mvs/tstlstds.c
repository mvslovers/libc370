/*
 * tstlstds.c - libc370 #50: DSLIST.catnm, measured on MVS.
 *
 * The host test (test/host/tstlstds.c) feeds parse() the LISTCAT lines as
 * IDCAMS printed them to SYSPRINT (JOB01086).  What it cannot answer: whether
 * the records __listc() reads back from its OUTFILE carry the same IN-CAT
 * line - carriage control, record format and all.  This runs the real
 * __listds() for each level in PARM and checks:
 *
 *   - every record has a catalog name (none NULL, none empty);
 *   - one level, one catalog: the names of all records are the same string
 *     and the same POINTER (shared, not one copy per record);
 *   - __freeds() returns, with the list cleared.
 *
 * It prints the name per level, so the run says which catalog answered:
 * on mvsdev IBMUSER is an alias of UCPUB000 and SYS2 lives in the master
 * catalog (JOB01086).
 *
 * PARM='LEVEL1 LEVEL2 ...'  (default IBMUSER SYS2)
 *
 * BUILD (host), against the branch library - -L build/sdk is load-bearing,
 * see test/mvs/tstfprls.c:
 *
 *     make build
 *     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstlstds.c \
 *           -o TSTLSTDS -flinker-output=iebcopy
 *     ld370 --pack TSTLSTDS=TSTLSTDS.iebcopy -o probe -xmit \
 *           --dsn IBMUSER.LIBC370.LSTDSSCR
 *
 * Then upload probe.xmit to IBMUSER.LIBC370.LSTDSXMT, run jcl/recvlstd.jcl,
 * run jcl/tstlstds.jcl.
 *
 * MEASURED 2026-10-01 on mvsdev, JOB01088, CC 0000, TSTLSTDS PASSED:
 *     LEVEL('IBMUSER')  30 records, all UCPUB000, one shared string
 *     LEVEL('SYS2')     11 records, all SYS1.VSAM.MASTER.CATALOG
 *     __freeds() ok on both.
 *
 * RC: 0 = every check passed, 8 = at least one did not (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include "ext/array.h"
#include "mvs/dslist.h"

static int bad = 0;

static void one_level(const char *level)
{
    DSLIST    **list;
    const char *first = NULL;
    unsigned    n, i;
    unsigned    nulls = 0, empty = 0, other = 0, copies = 0;

    printf("LEVEL('%s')\n", level);
    list = __listds(level, "NONVSAM VOLUME", NULL);
    n = list ? arraycount(&list) : 0;
    if (!n) {
        printf("    no records - nothing measured\n");
        bad++;
        if (list) __freeds(&list);
        return;
    }

    for (i = 0; i < n; i++) {
        const char *c = list[i]->catnm;

        if (i < 3) printf("    %-44s %-6s %s\n", list[i]->dsn,
                          list[i]->volser, c ? c : "(null)");
        if (!c)       { nulls++; continue; }
        if (!c[0])    { empty++; continue; }
        if (!first)   { first = c; continue; }
        if (strcmp(c, first)) other++;
        else if (c != first) copies++;
    }

    printf("    %u record(s), catalog %s\n", n, first ? first : "(none)");
    printf("    NULL %u, empty %u, other catalog %u, unshared copies %u\n",
           nulls, empty, other, copies);
    if (nulls || empty) { printf("    *** FAIL - records without a name\n"); bad++; }
    if (other)  { printf("    *** FAIL - one level, several catalogs\n"); bad++; }
    if (copies) { printf("    *** FAIL - the name is not shared\n"); bad++; }

    __freeds(&list);
    if (list) { printf("    *** FAIL - __freeds() left the list\n"); bad++; }
    else printf("    __freeds() ok\n");
}

int main(int argc, char **argv)
{
    int i;

    printf("TSTLSTDS - libc370 #50 probe\n\n");
    if (argc > 1) {
        for (i = 1; i < argc; i++) one_level(argv[i]);
    }
    else {
        one_level("IBMUSER");
        one_level("SYS2");
    }

    printf("\nTSTLSTDS %s\n", bad ? "FAILED" : "PASSED");
    return bad ? 8 : 0;
}
