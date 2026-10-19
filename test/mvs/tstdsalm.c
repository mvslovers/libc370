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
 *
 * and the rest of #408, on IBMUSER.LIBC370.T408P (members MEMA, MEMB)
 * and IBMUSER.LIBC370.T408S, which the job's first steps write:
 *
 *   4 __dsalc() DSN=A.B(MEM) allocates the member: the first record
 *     read is the member's.  The red made the name "A.B=MEM", which SVC
 *     99 refuses.
 *   5 __dsalc() FREE=CLOSE: the DD is there after the allocation and
 *     gone after fclose() (no DSAB, and __dsfree() finds nothing).  The
 *     red ignored the keyword.
 *     Once with a member and once with a sequential data set, whose
 *     allocation the red could make.
 *   6 control: without FREE=CLOSE the DD stays after fclose().
 *   7 __dynal() with "A.B(MEM)" allocates the member (the red passed the
 *     name to SVC 99 as it was), and its DD goes at fclose().
 *   8 __dynal() of a plain name: its DD goes at fclose() (FREE=CLOSE, which
 *     the red defined but never passed).
 *
 * The UNCATLG/UNCAT order, the space forms, the member and FREE=CLOSE text
 * units are pinned by the host test test/host/tstdsnmt.c.
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
#include <stdio.h>
#include <string.h>
#include <mvs/wto.h>
#include <mvs/dynalloc.h>
#include <mvs/dscb.h>
#include <mvs/dd.h>

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

/* the first record of DD dd, its first 9 characters */
static int first(const char *dd, char *line)
{
    char    name[12];
    FILE    *fp;
    int     ok = 0;

    line[0] = 0;
    snprintf(name, sizeof(name), "DD:%s", dd);
    fp = fopen(name, "r");
    if (!fp) return 0;
    if (fgets(line, 10, fp)) ok = 1;
    fclose(fp);
    return ok;
}

/* DD dd allocated: a DSAB, and __dsfree() as the second opinion when
   release is set (it frees it) */
static int there(const char *dd, int release)
{
    int dsab = get_dsab(NULL, dd) != NULL;
    int rc;

    if (!release) return dsab;
    rc = __dsfree(dd);
    wtof("TSTDSALM   DD %s: DSAB %s, __dsfree() rc %d", dd,
         dsab ? "found" : "none", rc);
    return dsab || rc == 0;
}

static void member(const char *opts, const char *expect, int freeclose)
{
    char    dd[9];
    char    line[12];
    int     rc;

    memset(dd, 0, sizeof(dd));
    rc = __dsalc(dd, opts);
    wtof("TSTDSALM   __dsalc(%s): rc %d, DD %s", opts, rc, dd);
    want(rc == 0, "__dsalc() allocates");
    if (rc) return;
    want(there(dd, 0), "the DD is there before the close");
    rc = first(dd, line);
    wtof("TSTDSALM   first record of %s: %s", dd, rc ? line : "(none)");
    want(rc && strncmp(line, expect, strlen(expect)) == 0,
         "the member's own first record");
    if (freeclose)
        want(!there(dd, 1), "FREE=CLOSE: the DD is gone after fclose()");
    else
        want(there(dd, 1), "control: without FREE=CLOSE the DD stays");
}

static void dynal(const char *dsn, const char *expect, const char *what)
{
    char    dd[9];
    char    line[12];
    int     rc;

    memset(dd, ' ', 8);
    dd[8] = 0;
    rc = __dynal(8, dd, strlen(dsn), (char *)dsn);
    wtof("TSTDSALM   __dynal(%s): rc %08X, DD %s", dsn, rc, dd);
    want(rc == 0, what);
    if (rc) return;
    rc = first(dd, line);
    wtof("TSTDSALM   first record of %s: %s", dd, rc ? line : "(none)");
    want(rc && strncmp(line, expect, strlen(expect)) == 0,
         "__dynal(): the data set's own first record");
    want(!there(dd, 1), "__dynal(): FREE=CLOSE, the DD is gone after fclose()");
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

    member("dsn=IBMUSER.LIBC370.T408P(MEMA);disp=shr", "MEMA LINE", 0);
    member("dsn=IBMUSER.LIBC370.T408P(MEMB);disp=shr;free=close",
           "MEMB LINE", 1);
    member("dsn=IBMUSER.LIBC370.T408S;disp=shr;free=close", "SEQ LINE", 1);
    dynal("IBMUSER.LIBC370.T408P(MEMB)", "MEMB LINE",
          "__dynal() of A.B(MEM) allocates");
    dynal("IBMUSER.LIBC370.T408S", "SEQ LINE",
          "__dynal() of a plain name allocates");
    return bad ? 8 : 0;
}
