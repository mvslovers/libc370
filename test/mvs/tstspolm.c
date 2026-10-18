/*
 * tstspolm.c - libc370 #406 on MVS: the spool and checkpoint helpers.
 *
 *   1 __cpopen("dd:CKPT") opens a DD whose name is shorter than 8
 *     characters (the name took its NUL and what follows, and OPEN failed)
 *   2 __cpread() past the last block returns -1 (CHECK abended S337: no
 *     EODAD).  The "checkpoint" is a data set of ten 4096-byte RECFM=U
 *     blocks of zeros that the job creates; DD HASPCKPT, 8 characters, so
 *     that the old parsing opens it too
 *   3 __jsopen("dd:NOSUCH") for a DD that is not there returns NULL (it
 *     went on through NULL after the failed OPEN: S0C4)
 *
 * CASE=0 runs all three (this tree); the red builds against the installed
 * library run one case each, CASE=1..3.
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -DCASE=0 -Iinclude -L build/sdk \
 *         test/mvs/tstspolm.c -o $S/TSTSPOLG -flinker-output=iebcopy
 *     (CASE=1..3 without -Iinclude/-L as TSTSPOR1..3)
 *     cd $S && ld370 --pack TSTSPOLG=TSTSPOLG.iebcopy ... -o sp -xmit \
 *         --dsn IBMUSER.LIBC370.SPSCR
 *
 * then jcl/recvspolm.jcl and jcl/tstspolm.jcl.
 *
 * RC 0 = every case passed, 8 = one did not.
 */
#include <stdlib.h>
#include <mvs/wto.h>
#include <mvs/jes2ckpt.h>
#include <mvs/jes2spool.h>

#ifndef CASE
#define CASE 0
#endif

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTSPOLM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

int main(void)
{
#if CASE == 0 || CASE == 1
    {
        HASPCP  *cp = __cpopen("dd:CKPT");

        want(cp != NULL, "1 __cpopen() of a 4-character DD name opens");
        if (cp) __cpclos(cp);
    }
#endif
#if CASE == 0 || CASE == 2
    {
        HASPCP  *cp = __cpopen("dd:HASPCKPT");
        char    *buf = malloc(4096);
        int     n = 0, rc = 0;

        if (cp && buf) {
            while (n < 20 && (rc = __cpread(cp, buf)) == 0) n++;
            wtof("TSTSPOLM %d more blocks, then rc %d", n, rc);
        }
        want(cp && rc == -1 && n < 20, "2 __cpread() at the end returns -1");
        if (cp) __cpclos(cp);
        free(buf);
    }
#endif
#if CASE == 0 || CASE == 3
    {
        HASPJS  *js = __jsopen("dd:NOSUCH");

        want(js == NULL, "3 __jsopen() of a missing DD returns NULL");
    }
#endif
    wtof("TSTSPOLM VERDICT failed=%d", bad);
    return bad ? 8 : 0;
}
