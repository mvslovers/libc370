/*
 * tstsdwfm.c - libc370 #640 on MVS: try() frees the SDWA of an abend it
 * retries.
 *
 * failed(), try()'s recovery exit, asked RTM for the retry with SETRP but
 * never set SDWAFREE, so the SDWA stayed allocated after every retried
 * abend - about 520 bytes of the region each time, until the address space
 * ended (measured by OFTPD, oftpd#13).
 *
 * The probe sums the free storage of the region: GETMAIN VC (conditional,
 * 64 bytes to 16 MB) until nothing is left, the blocks chained through
 * their first word, then FREEMAIN of all of them.
 *
 *   1 control: 50 try() calls that return normally lose nothing
 *   2 50 try() calls that abend S0C1 lose at most 64 bytes in all
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstsdwfm.c \
 *         -o $S/TSTSDWFG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstsdwfm.c \
 *         -o $S/TSTSDWFR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTSDWFG=TSTSDWFG.iebcopy \
 *         TSTSDWFR=TSTSDWFR.iebcopy -o sf -xmit --dsn IBMUSER.LIBC370.SFSCR
 *
 * then jcl/recvsdwfm.jcl and jcl/tstsdwfm.jcl.
 *
 * Own MVS/CE container, 2026-10-19: GREEN 0 bytes lost; RED (2.22.0)
 * 25,640 bytes after 50 abends, 512 per abend.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <mvs/wto.h>
#include <mvs/recovery.h>

#define ROUNDS  50

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTSDWFM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* the free storage of the region, in bytes; every block is given back */
static unsigned freesum(void)
{
    unsigned    la[2];
    unsigned    got[2];
    unsigned    *head = 0, *b, *next;
    unsigned    total = 0;
    int         rc;
    int         n = 0;

    la[0] = 64;
    la[1] = 0x00FFFFF8;
    for (;;) {
        /* volatile: an asm with an output and the same inputs every time
           is otherwise computed once, outside the loop */
        __asm__ __volatile__("GETMAIN VC,LA=(%1),A=(%2),SP=0\n\tLR\t%0,15"
                : "=r"(rc) : "r"(la), "r"(got)
                : "0", "1", "14", "15", "memory");
        if (rc != 0 || ++n > 20000) break;
        b = (unsigned *)got[0];
        b[0] = (unsigned)head;
        b[1] = got[1];
        head = b;
        total += got[1];
    }
    for (b = head; b; b = next) {
        unsigned len = b[1];

        next = (unsigned *)b[0];
        __asm__ __volatile__("FREEMAIN R,LV=(%0),A=(%1),SP=0"
                : : "r"(len), "r"(b) : "0", "1", "14", "15", "memory");
    }
    return total;
}

static int fine(void)
{
    return 0;
}

static int boom(void)
{
    __asm__("DC\tH'0'");
    return 0;
}

int main(void)
{
    unsigned    a, b, c;
    int         i, rc, abends = 0;

    try(fine, 0);               /* the first try() sets up what it keeps */
    try(boom, 0);
    a = freesum();
    for (i = 0; i < ROUNDS; i++) try(fine, 0);
    b = freesum();
    for (i = 0; i < ROUNDS; i++) {
        rc = try(boom, 0);
        if (rc == 0x0C1000) abends++;
    }
    c = freesum();
    wtof("TSTSDWFM   free %u, after %d normal %u, after %d abends %u",
         a, ROUNDS, b, abends, c);
    wtof("TSTSDWFM   lost: %d normal, %d with abends (%d per abend)",
         (int)(a - b), (int)(b - c), (int)(b - c) / (abends ? abends : 1));
    want(abends == ROUNDS, "every boom() came back as S0C1");
    want((int)(a - b) <= 64, "normal try() calls lose nothing");
    want((int)(b - c) <= 64, "abends retried by try() lose nothing");
    return bad ? 8 : 0;
}
