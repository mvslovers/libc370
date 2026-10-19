/*
 * tstm0m.c - libc370 #394 on MVS: malloc(0), calloc(0, n) and calloc(n, 0)
 * return NULL without ENOMEM and without the console message.
 *
 * C99 7.20.3 allows NULL for a request of 0 bytes; it is not a shortage.
 * The library before the fix set ENOMEM and wrote "Out of memory, bytes
 * needed=0" and a traceback to the console for each of them.  The probe
 * checks errno; the job log shows the message (or its absence).
 *
 * BUILD (from the repository root, after make; S a scratch directory,
 * MAIN a build of origin/main for the red control):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstm0m.c \
 *         -o $S/TSTM0G -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror -I$MAIN/include -L $MAIN/build/sdk \
 *         test/mvs/tstm0m.c -o $S/TSTM0R -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTM0G=TSTM0G.iebcopy \
 *         TSTM0R=TSTM0R.iebcopy -o m0 -xmit --dsn IBMUSER.LIBC370.MZ0SCR
 *
 * then jcl/recvm0m.jcl and jcl/tstm0m.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <stdlib.h>
#include <errno.h>
#include <mvs/wto.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTM0M %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* volatile: the sizes must reach the library as they are */
static volatile size_t zero = 0;
static volatile size_t five = 5;

int main(void)
{
    void    *p;

    errno = 0;
    p = malloc(zero);
    wtof("TSTM0M malloc(0)=%p errno=%d", p, errno);
    want(p == NULL && errno == 0, "malloc(0): NULL, errno unchanged");

    errno = 0;
    p = calloc(zero, five);
    wtof("TSTM0M calloc(0,5)=%p errno=%d", p, errno);
    want(p == NULL && errno == 0, "calloc(0, 5): NULL, errno unchanged");

    errno = 0;
    p = calloc(five, zero);
    wtof("TSTM0M calloc(5,0)=%p errno=%d", p, errno);
    want(p == NULL && errno == 0, "calloc(5, 0): NULL, errno unchanged");

    errno = 0;
    p = malloc(five);
    want(p != NULL && errno == 0, "control: malloc(5) succeeds");
    free(p);

    wtof("TSTM0M VERDICT failed=%d", bad);
    return bad ? 8 : 0;
}
