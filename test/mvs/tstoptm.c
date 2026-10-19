/*
 * tstoptm.c - libc370 #171 on MVS: the fopen() and __dsalc() option
 * parsers after the overlapping strcpy(p, p+1) became memmove().
 *
 * The overlapping copy was undefined in C but did the right thing with
 * the target's forward strcpy(), so this is a guard that nothing changed:
 * green (this tree) and red (the installed library) must both pass.  The
 * defect itself is shown by the host tests tstfprls, tstfpunit and
 * tstdsnmt, which run under AddressSanitizer now and abort without the fix.
 *
 *   1 fopen() of a new data set with "space=trk(2,1)" (a '(' after a
 *     keyword value: becomes '=', the ')' is removed) and "unit=(sysda)"
 *     (a '(' after '=': removed) writes, closes and reads back a record
 *   2 __dsalc() through __dsalcf() with "DISP=(NEW,CATLG,DELETE)" and
 *     "SPACE=TRK(1,1)" creates a data set, which is then deleted
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstoptm.c \
 *         -o $S/TSTOPTG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstoptm.c \
 *         -o $S/TSTOPTR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTOPTG=TSTOPTG.iebcopy \
 *         TSTOPTR=TSTOPTR.iebcopy -o op -xmit --dsn IBMUSER.LIBC370.OPSCR
 *
 * then jcl/recvoptm.jcl and jcl/tstoptm.jcl.  The two data sets,
 * IBMUSER.LIBC370.OPTF and IBMUSER.LIBC370.OPTD, must not exist.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <stdio.h>
#include <string.h>
#include <mvs/wto.h>
#include <mvs/dynalloc.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTOPTM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

int main(void)
{
    static const char   rec[] = "TSTOPTM RECORD";
    char                buf[81];
    char                dd[9];
    FILE                *fp;
    int                 rc;

    fp = fopen("'IBMUSER.LIBC370.OPTF'",
               "wb,recfm=fb,lrecl=80,blksize=800,space=trk(2,1),unit=(sysda)");
    want(fp != NULL, "fopen() with space=trk(2,1),unit=(sysda)");
    if (fp) {
        memset(buf, ' ', 80);
        memcpy(buf, rec, sizeof(rec) - 1);
        want(fwrite(buf, 1, 80, fp) == 80, "write a record");
        want(fclose(fp) == 0, "fclose()");
        fp = fopen("'IBMUSER.LIBC370.OPTF'", "rb");
        memset(buf, 0, sizeof(buf));
        want(fp && fread(buf, 1, 80, fp) == 80 &&
             memcmp(buf, rec, sizeof(rec) - 1) == 0, "read the record back");
        if (fp) fclose(fp);
        want(remove("'IBMUSER.LIBC370.OPTF'") == 0, "remove it");
    }

    rc = __dsalcf(dd, "DSN=%s;DISP=(NEW,CATLG,DELETE);DSORG=PS;RECFM=FB;"
                  "LRECL=80;BLKSIZE=3120;UNIT=SYSDA;SPACE=TRK(1,1)",
                  "IBMUSER.LIBC370.OPTD");
    want(rc == 0, "__dsalcf() with DISP=(NEW,CATLG,DELETE),SPACE=TRK(1,1)");
    if (rc == 0) {
        __dsfree(dd);
        want(remove("'IBMUSER.LIBC370.OPTD'") == 0, "remove it");
    }
    return bad ? 8 : 0;
}
