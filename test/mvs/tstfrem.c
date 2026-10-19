/*
 * tstfrem.c - libc370 #179 on MVS: a freopen() that cannot open the new
 * data set leaves the old stream open and usable.
 *
 * __reopen() set the error indicator of the old stream when the new
 * fopen() failed.  Since #149 that indicator refuses every later read and
 * write, so a failed freopen() silently killed a healthy stream.
 *
 *   1 freopen() of a data set that does not exist returns NULL
 *   2 the old stream has no error indicator set
 *   3 a write to it afterwards succeeds
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstfrem.c \
 *         -o $S/TSTFREG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstfrem.c \
 *         -o $S/TSTFRER -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTFREG=TSTFREG.iebcopy \
 *         TSTFRER=TSTFRER.iebcopy -o fr -xmit --dsn IBMUSER.LIBC370.FRSCR
 *
 * then jcl/recvfrem.jcl and jcl/tstfrem.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <stdio.h>
#include <mvs/wto.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTFREM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

int main(void)
{
    FILE    *f = fopen("&&FR", "w");
    FILE    *r;
    int     w;

    if (!f) {
        want(0, "fopen &&FR");
        return 8;
    }
    fputs("one\n", f);
    r = freopen("'IBMUSER.NO.SUCH.DATASET'", "r", f);
    want(r == NULL, "freopen() of a missing data set returns NULL");
    want(!ferror(f), "the old stream has no error indicator");
    w = fputs("two\n", f);
    wtof("TSTFREM   ferror %d, fputs %d", ferror(f), w);
    want(w >= 0, "a write to the old stream succeeds");
    fclose(f);
    return bad ? 8 : 0;
}
