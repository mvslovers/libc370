/*
 * tstfclm.c - libc370 #174 on MVS: fclose() does not leave the FILE's ENQ
 * behind when the task already holds it.
 *
 * fclose() released the FILE lock only when its own lock() acquired it.
 * A task that already held it - a write that abended under the caller's
 * own recovery and never DEQ'd - kept the ENQ on storage fclose() freed.
 *
 * The probe takes the FILE lock itself, as the abended write would have
 * left it, calls fclose(), and then asks lock() for the same pointer
 * value: 0 means the ENQ is gone, 8 that the task still holds it.
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstfclm.c \
 *         -o $S/TSTFCLG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstfclm.c \
 *         -o $S/TSTFCLR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTFCLG=TSTFCLG.iebcopy \
 *         TSTFCLR=TSTFCLR.iebcopy -o fc -xmit --dsn IBMUSER.LIBC370.FCSCR
 *
 * then jcl/recvfclm.jcl and jcl/tstfclm.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <stdio.h>
#include <mvs/wto.h>
#include <mvs/lock.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTFCLM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

int main(void)
{
    FILE    *f = fopen("&&FC", "w");
    void    *thing;
    int     rc;

    if (!f) {
        want(0, "fopen &&FC");
        return 8;
    }
    fputs("data\n", f);
    thing = f;
    rc = lock(thing, 0);            /* what an abended write leaves */
    want(rc == 0, "the FILE lock taken");
    fclose(f);
    rc = lock(thing, 0);            /* 8: this task still holds it */
    wtof("TSTFCLM   lock() of the closed FILE's address: rc %d", rc);
    want(rc == 0, "no ENQ left on the closed FILE");
    if (rc == 0) unlock(thing, 0);
    return bad ? 8 : 0;
}
