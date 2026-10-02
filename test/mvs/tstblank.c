/*
 * tstblank.c - libc370 #314 on MVS: isblank(), as macro and as function,
 * is true for exactly ' ' and '\t' among all 256 byte values.
 *
 * test/host/tstblank.c checks the table by EBCDIC code point.  This run
 * checks it through the compiler's own ' ' and '\t', so it also pins that
 * cc370 encodes them as the two entries the table marks.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstblank.c -o TSTBLNK -flinker-output=iebcopy
 *          ld370 --pack TSTBLNK=TSTBLNK.iebcopy -o tstblank -xmit \
 *                --dsn IBMUSER.LIBC370.BLNKSCR
 * Install: jcl/recvblnk.jcl.   Run: jcl/tstblank.jcl.
 *
 * GREEN: mvsdev JOB01157, CC 0000, 10/10, 2026-10-02 (RECEIVE JOB01156).
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <ctype.h>

int (isblank)(int c);

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

int main(void)
{
    int c;
    int nmac = 0, nfun = 0, odd = 0;
    volatile int sp = ' ', ht = '\t';

    printf("=== tstblank: isblank on MVS (#314) ===\n\n");
    for (c = 0; c < 256; c++) {
        int m = isblank(c) != 0;
        int f = (isblank)(c) != 0;
        nmac += m;
        nfun += f;
        if (m != f || (m && c != sp && c != ht)) {
            odd++;
            printf("  odd: X'%02X' macro %d function %d\n", c, m, f);
        }
    }
    CHECK(isblank(sp) != 0, "macro: ' ' is blank");
    CHECK(isblank(ht) != 0, "macro: '\\t' is blank");
    CHECK((isblank)(sp) != 0, "function: ' ' is blank");
    CHECK((isblank)(ht) != 0, "function: '\\t' is blank");
    CHECK(nmac == 2, "macro: exactly two of 256");
    CHECK(nfun == 2, "function: exactly two of 256");
    CHECK(odd == 0, "macro and function agree, nothing else is blank");
    CHECK(isspace(sp) && isspace(ht), "both stay isspace");
    CHECK(!isblank('\n') && !isblank('\v') && !isblank('\r'),
          "\\n, \\v, \\r are space but not blank");
    CHECK(isblank(EOF) == 0, "EOF is not blank");

    printf("\n=== tstblank: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
