/*
 * tstgmain.c - libc370 #283/#401 on MVS: getmain()/freemain() report
 * without printf, and getmain() refuses a size whose rounded length does
 * not fit 24 bits.
 *
 * Built twice: TSTGMAIN against this tree's libc.a, TSTGMR against the
 * installed one (2.18.0) - the console lines of cases 2 and 3 must read
 * alike, case 4 is the #401 red (it asks GETMAIN for 0 bytes there).
 *
 *   1 getmain(100) and freemain(): storage, rc 0
 *   2 getmain(15 MB) fails: the console message
 *   3 freemain() of a block whose prefix says length 0: -1 and the
 *     console message, the block is then freed after its prefix is put
 *     back
 *   4 getmain(X'FFFFB9'): NULL
 *
 * RC 0 = every case passed, 8 = one did not.
 */
#include <string.h>
#include <mvs/wto.h>
#include <mvs/storage.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTGMAIN %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

int main(void)
{
    char        *p = getmain(100, 0);
    unsigned    save;

    want(p != NULL, "1 getmain(100)");
    if (p) {
        memset(p, 'x', 100);
        want(freemain(p) == 0, "1 freemain() rc 0");
    }

    p = getmain(15 * 1024 * 1024, 0);
    want(p == NULL, "2 getmain(15 MB) fails, the console says so");

    p = getmain(100, 0);
    if (p) {
        save = *(unsigned *)(p - 8);
        *(unsigned *)(p - 8) = save & 0xFF000000;   /* length 0 */
        want(freemain(p) == -1, "3 freemain() of a bad prefix: -1, the console says so");
        *(unsigned *)(p - 8) = save;
        want(freemain(p) == 0, "3 freed after the prefix is put back");
    }

    p = getmain(0x00FFFFB9, 0);
    want(p == NULL, "4 getmain(X'FFFFB9') is refused");

    wtof("TSTGMAIN VERDICT failed=%d", bad);
    return bad ? 8 : 0;
}
