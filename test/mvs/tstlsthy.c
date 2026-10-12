/*
 * tstlsthy.c - libc370 #519 on MVS: __listds() keeps a '-' in a name.
 *
 * ISSUE #519 (found through mvsMF's data set list on dev1, 2026-10-12):
 * the entry name after NONVSAM was read with the " -\n" tokens that also
 * step over LISTCAT's run of dashes, so IBMUSER.OFTPIN.NOT-CATE came back
 * as IBMUSER.OFTPIN.NOT, and the filter saw only that.
 *
 * jcl/tstlsthy.jcl allocates IBMUSER.LIBC370.LST-519 and
 * IBMUSER.LIBC370.LSTX519, runs this against the real LISTCAT output and
 * deletes both.  Built twice: TSTLSTHY against this tree's libc.a,
 * TSTLSTHR against 2.10.1's - the red control.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstlsthy.c -o TSTLSTHY -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude -L <2.10.1 sysroot>/lib \
 *                test/mvs/tstlsthy.c -o TSTLSTHR -flinker-output=iebcopy
 *          ld370 --pack TSTLSTHY=TSTLSTHY.iebcopy \
 *                --pack TSTLSTHR=TSTLSTHR.iebcopy \
 *                -o tstlsthy -xmit --dsn IBMUSER.LIBC370.LHSCR
 * Install: jcl/recvlsth.jcl.   Run: jcl/tstlsthy.jcl.
 *
 * dev1 JOB02148, 2026-10-12 (RECEIVE JOB02147): GREEN CC 0000, 3/3;
 * RED (2.10.1) CC 0001, 1/3 - listed IBMUSER.LIBC370.LST, filter 0.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <mvs/dslist.h>
#include <ext/array.h>

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

int main(void)
{
    DSLIST      **l;
    unsigned    n, i;
    int         hy = 0, plain = 0;
    char        msg[96];

    printf("=== tstlsthy: a '-' in a data set name (#519) ===\n");

    l = __listds("IBMUSER.LIBC370", "NONVSAM VOLUME", NULL);
    n = l ? arraycount(&l) : 0;
    for (i = 0; i < n; i++) {
        printf("    %s\n", l[i]->dsn);
        if (strcmp(l[i]->dsn, "IBMUSER.LIBC370.LST-519") == 0) hy++;
        if (strcmp(l[i]->dsn, "IBMUSER.LIBC370.LSTX519") == 0) plain++;
    }
    CHECK(plain == 1, "IBMUSER.LIBC370.LSTX519 listed");
    sprintf(msg, "IBMUSER.LIBC370.LST-519 listed whole (%d)", hy);
    CHECK(hy == 1, msg);
    if (l) __freeds(&l);

    l = __listds("IBMUSER.LIBC370", "NONVSAM VOLUME", "IBMUSER.LIBC370.LST-*");
    n = l ? arraycount(&l) : 0;
    sprintf(msg, "the filter LST-* finds it: %u record(s)", n);
    CHECK(n == 1 && strcmp(l[0]->dsn, "IBMUSER.LIBC370.LST-519") == 0, msg);
    if (l) __freeds(&l);

    printf("tstlsthy %d run, %d passed, %d failed\n", mbt_run, mbt_passed, mbt_failed);
    return mbt_failed ? 1 : 0;
}
