/*
 * tstrefus.c - libc370 #405/#407 on MVS: calls that the library now
 * refuses before it issues any service.
 *
 *   racf_logout(NULL), racf_logout(&p) with p NULL: -1, errno EINVAL (#405)
 *   ssct_remove(NULL): 4 (#407)
 *   ssvt_set() past ssvtfnum, ssvt_funcmap() of 256: EPERM (#407)
 *
 * Every case returns before a key switch or a RACINIT, so the probe runs
 * in problem state without APF.  There is no red run on MVS: against an
 * older library racf_logout(NULL) deletes the initiator's resting ACEE,
 * which later jobs inherit, and ssct_remove(NULL) stores into low core
 * in key 0.  test/host/tstraclo.c and tstsubsy.c are the reds.
 *
 * RC 0 = every case passed, 8 = one did not.
 */
#include <stdlib.h>
#include <errno.h>
#include <mvs/wto.h>
#include <mvs/racf.h>
#include <mvs/subsys.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTREFUS %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

int main(void)
{
    ACEE    *p = NULL;
    SSVT    *ssvt = calloc(1, sizeof(SSVT) + 4 * sizeof(void *));
    int     rc;

    errno = 0;
    rc = racf_logout(NULL);
    want(rc == -1 && errno == EINVAL, "racf_logout(NULL): -1, EINVAL");
    errno = 0;
    rc = racf_logout(&p);
    want(rc == -1 && errno == EINVAL, "racf_logout(&NULL): -1, EINVAL");

    want(ssct_remove(NULL) == 4, "ssct_remove(NULL): 4");

    if (ssvt) {
        ssvt->ssvtfnum = 4;
        want(ssvt_set(ssvt, 5, (void *)main) == EPERM, "ssvt_set(5 of 4): EPERM");
        ssvt->ssvtfnum = 256;
        want(ssvt_funcmap(ssvt, 256, 1) == EPERM, "ssvt_funcmap(256): EPERM");
        free(ssvt);
    }

    wtof("TSTREFUS VERDICT failed=%d", bad);
    return bad ? 8 : 0;
}
