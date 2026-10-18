/*
 * tstapfun.c - libc370 #402/#406 on MVS: an unauthorized caller gets an
 * error from the functions that issue MODESET, not ABEND S047.
 *
 *   1 super_do(f)                  EPERM, f not called
 *   2 super_key_do(PSWKEY0, f)     EPERM, f not called
 *   3 clib_auth_cde(cde)           EPERM, the CDE untouched
 *   4 jescanj() (through iefssreq) -1, errno EPERM - it reported
 *                                  success when the request never ran
 *
 * Problem state, no APF.  CASE=0 runs all four (this tree, green); the
 * red build against the installed library runs one case per step
 * (CASE=1..4), each of which ends S047 there.
 *
 * RC 0 = every case passed, 8 = one did not.
 */
#include <errno.h>
#include <string.h>
#include <mvs/wto.h>
#include <mvs/apf.h>
#include <mvs/jes2.h>
#include <ibm/mvs/ihacde.h>

#ifndef CASE
#define CASE 0
#endif

static int bad;
static int called;

static void want(int cond, const char *what)
{
    wtof("TSTAPFUN %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

static int f(void) __attribute__((unused));
static int f(void) { called++; return 0; }

int main(void)
{
    CDE     cde;
    int     rc;

    memset(&cde, 0, sizeof(cde));
#if CASE == 0 || CASE == 1
    called = 0;
    rc = super_do(f);
    want(rc == EPERM && !called, "1 super_do() unauthorized: EPERM");
#endif
#if CASE == 0 || CASE == 2
    called = 0;
    rc = super_key_do(PSWKEY0, f);
    want(rc == EPERM && !called, "2 super_key_do() unauthorized: EPERM");
#endif
#if CASE == 0 || CASE == 3
    rc = clib_auth_cde(&cde);
    want(rc == EPERM && cde.CDATTR2 == 0, "3 clib_auth_cde() unauthorized: EPERM");
#endif
#if CASE == 0 || CASE == 4
    errno = 0;
    rc = jescanj("NOSUCHJB", "JOB99999", 0);
    want(rc == -1 && errno == EPERM, "4 jescanj() unauthorized: -1, EPERM");
#endif
    (void)rc; (void)cde;
    wtof("TSTAPFUN VERDICT failed=%d", bad);
    return bad ? 8 : 0;
}
