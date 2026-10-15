/*
 * tstfx37.c - libc370 #539: __fx37() names the x37 an ENOSPC write stood for.
 *
 * Four data sets are filled until a write fails, and __fx37() is asked
 * before the stream is closed.  The secondary space quantity reaches it on
 * two routes, and each route is taken with and without a secondary:
 *
 *   (a) by name, TRK(1,0)  - DD without SPACE, quantity from the DSCB: D37
 *   (b) by name, TRK(1,1)  - the same, 16 extents:                      B37
 *   (c) DD:,     TRK(1,0)  - the DD's own SPACE, quantity from the JFCB: D37
 *   (d) DD:,     TRK(1,1)  - the same:                                  B37
 *   (g) PDS member by name, TRK(1,0,1):                                 D37
 *   (e) a stream that has not run out of space:                         0
 *
 * The expected codes are what MVS itself issues without libc370's x37 exit,
 * measured with IEBDG on the same shapes (mvsdev JOB02342: D37-04 and
 * B37-04 for sequential; JOB02343: E37-04 and D37-04 for a PDS member).
 *
 * Not a case: a PDS member whose secondary space is used up.  IFG0554T
 * abends that E37-04 at once (EDO08300: the volume sequence is still 1, as
 * a PDS gets no second volume), before the x37 exit and SYNAD, so the write
 * never returns - measured in this probe, mvsdev JOB02345, SE37-04.  It
 * needs try() around the write, not __fx37().
 *
 * PARM='<hlq>'   (default IBMUSER.TSTFX37) - creates <hlq>.A to .E and .G
 * and deletes them; none may exist when it starts.
 * RC 0 = every check passed, 8 = a check failed.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstfx37.c -o TSTFX37 -flinker-output=iebcopy
 *          ld370 --pack TSTFX37=TSTFX37.iebcopy \
 *                -o tstfx37 -xmit --dsn IBMUSER.LIBC370.FX37SCR
 * Upload:  tstfx37.xmit to IBMUSER.LIBC370.FX37XMIT (FB 80, binary)
 * Install: jcl/recvfx37.jcl.   Run: jcl/tstfx37.jcl.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <mvs/wto.h>
#include <mvs/file.h>
#include <mvs/dynalloc.h>

#define ATTRS   "DSN=%s;DISP=(NEW,CATLG,DELETE);DSORG=PS;RECFM=FB;"          \
                "LRECL=80;BLKSIZE=800;UNIT=SYSDA;SPACE=TRK(%s)"

#define ATTRPO  "DSN=%s;DISP=(NEW,CATLG,DELETE);DSORG=PO;RECFM=FB;"          \
                "LRECL=80;BLKSIZE=800;UNIT=SYSDA;SPACE=TRK(%s)"

#define LIMIT   200000

static int  bad = 0;

static void check(int cond, const char *what)
{
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", what);
    wtof("TSTFX37 %s %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* create dsn; byname: free the DD and reopen by name, else write through
   the DD.  full: fill until a write fails.  Returns __fx37() before close,
   or 0xFFFFFFFF when it cannot measure. */
static unsigned run(const char *dsn, const char *space, int byname, int full,
                    const char *member)
{
    char        dd[9];
    char        fname[52];
    char        rec[80];
    FILE        *fp;
    long        n = 0;
    int         eno = 0;
    unsigned    code;

    if (__dsalcf(dd, member ? ATTRPO : ATTRS, dsn, space)) {
        printf("      __dsalcf(%s) failed - CANNOT MEASURE\n", dsn);
        return 0xFFFFFFFFu;
    }
    if (byname) {
        __dsfree(dd);
        if (member) sprintf(fname, "'%s(%s)'", dsn, member);
        else        sprintf(fname, "'%s'", dsn);
    } else {
        sprintf(fname, "DD:%s", dd);
    }

    fp = fopen(fname, "wb");
    if (!fp) {
        printf("      fopen(%s) NULL - CANNOT MEASURE\n", fname);
        if (!byname) __dsfree(dd);
        remove(dsn);
        return 0xFFFFFFFFu;
    }

    memset(rec, 'X', sizeof(rec));
    errno = 0;
    while (n < (full ? LIMIT : 10)) {
        if (fwrite(rec, 1, sizeof(rec), fp) != sizeof(rec)) {
            eno = errno;
            break;
        }
        n++;
    }
    code = __fx37(fp);
    fclose(fp);
    if (!byname) __dsfree(dd);
    remove(dsn);

    printf("      %-22s TRK(%s) %-6s %6ld record(s) errno=%-2d __fx37=%08X\n",
           dsn, space, byname ? "name" : "DD", n, eno, code);
    wtof("TSTFX37 %s TRK(%s) %s n=%ld errno=%d fx37=%08X",
         dsn, space, byname ? "name" : "DD", n, eno, code);
    return code;
}

int main(int argc, char **argv)
{
    char    hlq[36];
    char    dsn[48];

    strcpy(hlq, (argc > 1 && argv[1][0]) ? argv[1] : "IBMUSER.TSTFX37");
    printf("=== tstfx37: #539 - __fx37() ===\n");

    sprintf(dsn, "%s.A", hlq);
    check(run(dsn, "1,0", 1, 1, 0) == 0x00D37004, "(a) by name, no secondary: D37-04");
    sprintf(dsn, "%s.B", hlq);
    check(run(dsn, "1,1", 1, 1, 0) == 0x00B37004, "(b) by name, 16 extents: B37-04");
    sprintf(dsn, "%s.C", hlq);
    check(run(dsn, "1,0", 0, 1, 0) == 0x00D37004, "(c) DD, no secondary: D37-04");
    sprintf(dsn, "%s.D", hlq);
    check(run(dsn, "1,1", 0, 1, 0) == 0x00B37004, "(d) DD, 16 extents: B37-04");
    sprintf(dsn, "%s.G", hlq);
    check(run(dsn, "1,0,1", 1, 1, "MEM") == 0x00D37004,
          "(g) PDS member, no secondary: D37-04");
    sprintf(dsn, "%s.E", hlq);
    check(run(dsn, "1,1", 1, 0, 0) == 0,          "(e) not full: 0");
    check(__fx37(NULL) == 0,                   "(e) NULL: 0");

    printf("=== tstfx37: %d check(s) failed ===\n", bad);
    wtof("TSTFX37 VERDICT failed=%d", bad);
    return bad ? 8 : 0;
}
