/*
 * tstlstcm.c - libc370 #410 on MVS: __listc() returns the IDCAMS return
 * code, and __listal() with an empty DD name lists every DD.
 *
 *   1 __listc() of a level with no entries returns IDCAMS's nonzero
 *     return code; it returned 0
 *   2 control: __listc() of SYS1 returns 0 and passes records to the
 *     callback
 *   3 __listal(NULL, "", 0) lists as many DDs as __listal(NULL, NULL, 0);
 *     next_dsab("") stopped after the first DD and its concatenation
 *
 * The step needs at least two DD statements besides STEPLIB
 * (jcl/tstlstcm.jcl has DD1 and DD2).
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstlstcm.c \
 *         -o $S/TSTLSTCG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstlstcm.c \
 *         -o $S/TSTLSTCR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTLSTCG=TSTLSTCG.iebcopy \
 *         TSTLSTCR=TSTLSTCR.iebcopy -o lc -xmit --dsn IBMUSER.LIBC370.LCSCR
 *
 * then jcl/recvlstcm.jcl and jcl/tstlstcm.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <stdarg.h>
#include <mvs/wto.h>
#include <mvs/dslist.h>
#include <ext/array.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTLSTCM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* counts the records __listc() hands over */
static int count(void *udata, const char *fmt, ...)
{
    (void)fmt;
    (*(int *)udata)++;
    return 0;
}

static unsigned dds(const char *ddname)
{
    ALCLIST     **al = __listal(NULL, ddname, 0);
    unsigned    n    = al ? arraycount(&al) : 0;

    if (al) __freeal(&al);
    return n;
}

int main(void)
{
    int         rc, n;
    unsigned    all, empty;

    n = 0;
    rc = __listc("LIBC370.NOSUCH.LEVEL", NULL, count, &n);
    wtof("TSTLSTCM   __listc(no such level) rc %d, %d records", rc, n);
    want(rc > 0, "__listc() of an empty level returns IDCAMS's nonzero rc");

    n = 0;
    rc = __listc("SYS1", NULL, count, &n);
    wtof("TSTLSTCM   __listc(SYS1) rc %d, %d records", rc, n);
    want(rc == 0 && n > 0, "control: __listc(SYS1) rc 0 with records");

    all = dds(NULL);
    empty = dds("");
    wtof("TSTLSTCM   __listal(NULL) %u entries, __listal(\"\") %u", all, empty);
    want(all >= 3, "control: the step has STEPLIB, DD1 and DD2");
    want(empty == all, "__listal(\"\") lists every DD, as NULL does");
    return bad ? 8 : 0;
}
