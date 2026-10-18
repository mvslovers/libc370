/*
 * tstascbm.c - libc370 #403 on MVS: __ascb() of the highest ASID.
 *
 * ASIDs run from 1 to ASVTMAXU; __ascb() treated ASVTMAXU as out of
 * range.  The difference shows only while that ASID is in use, so the
 * job first fills the free ASIDs with started tasks that wait (CASE=9
 * builds the waiter, TSTASWT), then asks __ascb() for every ASID and
 * compares with the ASVT itself.
 *
 *   1 __ascb(n) is the ASCB for every ASID 1..ASVTMAXU in use, and NULL
 *     for one that is free; in particular for ASVTMAXU
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstascbm.c \
 *         -o $S/TSTASCBG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstascbm.c \
 *         -o $S/TSTASCBR -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror -DCASE=9 test/mvs/tstascbm.c \
 *         -o $S/TSTASWT -flinker-output=iebcopy
 *
 * The started tasks need a procedure in a procedure library:
 *
 *     //TSTASWT  PROC
 *     //WAIT     EXEC PGM=TSTASWT
 *     //STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.ASSCR
 *
 * then S TSTASWT.W1, .W2, ... until D A,L shows ASVTMAXU minus the
 * address spaces already there, and jcl/tstascbm.jcl within three minutes.
 *
 * RED, 2026-10-18, against 2.18.1, MVS/CE with ASVTMAXU 32 and all 32
 * taken: __ascb(32) NULL where the ASVT holds an ASCB.  GREEN: all 32.
 *
 * RC 0 = passed, 8 = failed, 4 = ASVTMAXU was not in use (no verdict).
 */
#include <string.h>
#include <mvs/wto.h>
#include <mvs/xmem.h>
#include <mvs/ecb.h>
#include <ibm/mvs/ihaasvt.h>

#ifndef CASE
#define CASE 0
#endif

#if CASE == 9
int main(void)
{
    ECB ecb = 0;

    /* hold an ASID for three minutes */
    ecb_timed_wait(&ecb, 18000, 0);
    return 0;
}
#else
static ASVT *asvt(void)
{
    unsigned    *psa = (unsigned *)0;
    unsigned    *cvt = (unsigned *)psa[0x10/4];

    return (ASVT *)cvt[0x22C/4];
}

/* the ASCB the ASVT holds for asid, or NULL when the entry is free */
static void *entry(ASVT *a, unsigned asid)
{
    unsigned    e = (unsigned)a->asvtenty[asid - 1];

    if (e & 0x80000000) return NULL;
    if (memcmp((void *)e, "ASCB", 4) != 0) return NULL;
    return (void *)e;
}

int main(void)
{
    ASVT        *a = asvt();
    unsigned    max = a->asvtmaxu;
    unsigned    n, used = 0, wrong = 0;

    for (n = 1; n <= max; n++) {
        void *want = entry(a, n);
        void *got  = __ascb(n);

        if (want) used++;
        if (got != want) {
            wrong++;
            wtof("TSTASCBM ASID %u: ASVT %08X, __ascb %08X", n,
                 (unsigned)want, (unsigned)got);
        }
    }
    wtof("TSTASCBM ASVTMAXU %u, %u in use, highest %s", max, used,
         entry(a, max) ? "in use" : "free");
    if (!entry(a, max)) {
        wtof("TSTASCBM NO VERDICT: ASID %u is free", max);
        return 4;
    }
    wtof("TSTASCBM %s: 1 __ascb() agrees with the ASVT for 1..%u",
         wrong ? "FAIL" : "PASS", max);
    return wrong ? 8 : 0;
}
#endif
