/*
 * tstdsalm.c - libc370 #408 on MVS: __dsalc() takes the JCL form of
 * SPACE= and leaves the caller's strtok() alone.
 *
 *   1 SPACE=(TRK,(1,2)) allocates a new data set (it failed with 1)
 *   2 control: SPACE=TRK(1,2)
 *   3 __dsalc(dd, NULL) does not go on with the caller's pending strtok():
 *     the red took the caller's "DSN=...;DISP=(NEW,CATLG)" as its own
 *     options and cataloged IBMUSER.T408.EVIL, which LOCATE finds.  It
 *     restored the caller's strtok() position, so the next token looks
 *     the same either way.
 *
 * The UNCATLG/UNCAT order and the space forms are pinned by the host
 * test test/host/tstdsnmt.c.
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstdsalm.c \
 *         -o $S/TSTDSAG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstdsalm.c \
 *         -o $S/TSTDSAR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTDSAG=TSTDSAG.iebcopy \
 *         TSTDSAR=TSTDSAR.iebcopy -o da -xmit --dsn IBMUSER.LIBC370.DASCR
 *
 * then jcl/recvdsalm.jcl and jcl/tstdsalm.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <string.h>
#include <mvs/wto.h>
#include <mvs/dynalloc.h>
#include <mvs/dscb.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTDSALM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

static void alloc(const char *opts, const char *what)
{
    char    dd[9];
    int     rc;

    memset(dd, 0, sizeof(dd));
    rc = __dsalc(dd, opts);
    wtof("TSTDSALM   %s: rc %d, DD %s", what, rc, dd);
    want(rc == 0, what);
    if (rc == 0) __dsfree(dd);
}

int main(void)
{
    char    mine[] = "A=1;DSN=IBMUSER.T408.EVIL;DISP=(NEW,CATLG);NEXT";
    char    dd[9];
    char    *t;

    alloc("dsn=&&T408A;disp=(new,delete);space=(trk,(1,2));unit=sysda",
          "SPACE=(TRK,(1,2))");
    alloc("dsn=&&T408B;disp=(new,delete);space=trk(1,2);unit=sysda",
          "control: SPACE=TRK(1,2)");

    t = strtok(mine, ";");
    memset(dd, 0, sizeof(dd));
    __dsalc(dd, NULL);
    if (dd[0]) __dsfree(dd);
    t = strtok(NULL, ";");
    wtof("TSTDSALM   the caller's next token: %s", t ? t : "(null)");
    want(t != NULL && strcmp(t, "DSN=IBMUSER.T408.EVIL") == 0,
         "__dsalc(dd, NULL): the caller's next token is its own");
    {
        LOCWORK     work;
        char        dsn[44];
        int         rc;

        memset(&work, 0, sizeof(work));
        memset(dsn, ' ', sizeof(dsn));
        memcpy(dsn, "IBMUSER.T408.EVIL", 17);
        rc = __locate(dsn, &work);
        wtof("TSTDSALM   LOCATE IBMUSER.T408.EVIL: rc %d", rc);
        want(rc != 0, "__dsalc(dd, NULL) allocated nothing from the caller's string");
    }
    return bad ? 8 : 0;
}
