/*
 * tstfplm.c - libc370 #114 on MVS: osbclose() releases the buffer pool
 * OPEN built, and leaves a DCB without one alone.
 *
 * DD INDD has DCB=BUFNO=3, so OPEN builds a pool and puts its address in
 * DCBBUFCB.  DD INNP has no BUFNO: no pool, DCBBUFCB keeps the DCB
 * macro's X'000001'.  FREEPOOL FREEMAINs whatever DCBBUFCB points to and
 * then sets its low-order bit ("no pool"); it must not run without a pool.
 *
 *   1 INDD: after OPEN DCBBUFCB is a pool address (nonzero, even)
 *   2 after osbclose(keep DCB) its low-order bit is on: the pool was
 *     released (CLOSE alone leaves the address in place)
 *   3 the same DCB opens again (as ufsd's formatter does), gets a pool of
 *     its own, reads a block, closes
 *   4 40 cycles osbdcb/osbopen/osbclose(free): released pools are reused,
 *     so at most a few distinct pool addresses; without the FREEPOOL each
 *     cycle orphans its pool and every address is new
 *   5 INNP: open, read, close without a pool - no abend, DCBBUFCB still
 *     X'000001'
 *
 * The data set IBMUSER.LIBC370.FPLDATA is created by jcl/tstfplm.jcl.
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstfplm.c \
 *         -o $S/TSTFPLG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstfplm.c \
 *         -o $S/TSTFPLR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTFPLG=TSTFPLG.iebcopy \
 *         TSTFPLR=TSTFPLR.iebcopy -o fp -xmit --dsn IBMUSER.LIBC370.FPSCR
 *
 * then jcl/recvfplm.jcl and jcl/tstfplm.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <stdlib.h>
#include <string.h>
#include <mvs/wto.h>
#include <mvs/osio.h>

#define CYCLES 40

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTFPLM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

static unsigned bufcb(DCB *dcb)
{
    return (unsigned)dcb->dcbbufcb & 0x00FFFFFF;
}

/* read the first block; 0 when it arrived */
static int readone(DCB *dcb)
{
    static char buf[3200];
    DECB        decb;

    memset(&decb, 0, sizeof(decb));
    osread(&decb, dcb, buf, sizeof(buf));
    return oscheck(&decb);
}

int main(void)
{
    static unsigned seen[CYCLES];
    DCB             *dcb;
    unsigned        b, i, j, distinct = 0;

    /* 1-3: one DCB, opened twice */
    dcb = osbdcb("INDD", NULL);
    want(dcb && osbopen(dcb, 0, "r") == 0, "INDD opens");
    if (!dcb) return 8;
    b = bufcb(dcb);
    wtof("TSTFPLM   INDD after OPEN  DCBBUFCB %06X", b);
    want(b && !(b & 1), "OPEN built a pool (BUFNO=3 on the DD)");
    osbclose(dcb, NULL, 0, 0);
    wtof("TSTFPLM   INDD after CLOSE DCBBUFCB %06X", bufcb(dcb));
    want(bufcb(dcb) & 1, "after osbclose() DCBBUFCB says no pool");

    want(osbopen(dcb, 0, "r") == 0, "the same DCB opens again");
    b = bufcb(dcb);
    wtof("TSTFPLM   INDD reopened    DCBBUFCB %06X", b);
    want(b && !(b & 1), "the reopen built a pool of its own");
    want(readone(dcb) == 0, "and reads a block");
    osbclose(dcb, NULL, 1, 0);

    /* 4: a new DCB per cycle, as __renmem() and the formatter use it */
    for (i = 0; i < CYCLES; i++) {
        dcb = osbdcb("INDD", NULL);
        if (!dcb || osbopen(dcb, 0, "r")) { seen[i] = 0; continue; }
        seen[i] = bufcb(dcb);
        osbclose(dcb, NULL, 1, 0);
    }
    for (i = 0; i < CYCLES; i++) {
        for (j = 0; j < i && seen[j] != seen[i]; j++) ;
        if (j == i) distinct++;
    }
    wtof("TSTFPLM   %d cycles: %u distinct pool addresses, first %06X last %06X",
         CYCLES, distinct, seen[0], seen[CYCLES - 1]);
    want(seen[0] && distinct <= 3, "released pools are reused");

    /* 5: no pool at all */
    dcb = osbdcb("INNP", NULL);
    want(dcb && osbopen(dcb, 0, "r") == 0, "INNP opens");
    if (!dcb) return 8;
    wtof("TSTFPLM   INNP after OPEN  DCBBUFCB %06X", bufcb(dcb));
    want(readone(dcb) == 0, "INNP reads a block");
    osbclose(dcb, NULL, 0, 0);
    want(bufcb(dcb) == 1, "INNP: no pool, DCBBUFCB still X'000001'");
    free(dcb);
    return bad ? 8 : 0;
}
