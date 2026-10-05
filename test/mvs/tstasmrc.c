/*
 * tstasmrc.c - libc370 #425 and #427 on MVS: asm statements that write
 * memory say so.
 *
 * ISSUE #425: memset() (<string.h>) and memclr() (<ext/strutil.h>) are
 * inlines whose MVCL wrote the target without telling the compiler, so a
 * value stored there before the call could come back after it - at -Os,
 * -O1 and -O2 (x = 5; memset(&x, 0, 4); return x; returned 5).
 *
 * ISSUE #427: clib_identify_cthread() stored IDENTIFY's R15 through a
 * pointer the asm took as an input, and returned the 0 rc was initialised
 * with.  Its second call in a program must return 4 (name exists; measured
 * on mvsdev JOB01368).
 *
 * The memset/memclr half is in the header, so the red control compiles
 * this source against main's string.h/strutil.h and links the installed
 * libc.a; the green one uses this tree for both.
 *
 * Build:   make build
 *          cc370 -Os -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstasmrc.c -o TSTASMR -flinker-output=iebcopy
 *          mkdir -p old/ext && cp -R include/ old/ && \
 *              git show 54e262e:include/string.h > old/string.h && \
 *              git show 54e262e:include/ext/strutil.h > old/ext/strutil.h
 *          cc370 -Os -Wall -Werror -Iold \
 *                test/mvs/tstasmrc.c -o TSTASMRR -flinker-output=iebcopy
 *          ld370 --pack TSTASMR=TSTASMR.iebcopy TSTASMRR=TSTASMRR.iebcopy \
 *                -o tstasmrc -xmit --dsn IBMUSER.LIBC370.ASMSCR
 * Install: jcl/recvasm.jcl.   Run: jcl/tstasmrc.jcl.
 *
 * mvsdev JOB01372, 2026-10-05 (RECEIVE JOB01371): GREEN CC 0000, 5/5;
 * RED (main's headers, installed 2.2.0 libc.a) CC 0001, 0/5
 * (clib_identify_cthread() returned 0).
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <ext/strutil.h>

int clib_identify_cthread(void) asm("@@IDECTH");

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

#define NOINL __attribute__((noinline))

/* the shapes the bug needs: a store the compiler can forward, then the
   inline, then a read (and a store it could sink past the inline) */
static NOINL int ms_readback(void)
{
    int x;

    x = 5;
    memset(&x, 0, sizeof x);
    return x;
}

static NOINL int ms_char(void)
{
    char c[4];

    c[0] = 'A';
    memset(c, 'B', sizeof c);
    return c[0];
}

static NOINL int ms_loop(int n)
{
    int v[4];
    int i;

    for (i = 0; i < n; i++) {
        v[0] = i + 1;
        memset(v, 0, sizeof v);
    }
    return v[0];
}

static NOINL int mc_readback(void)
{
    int x;

    x = 7;
    memclr(&x, sizeof x);
    return x;
}

static volatile int three = 3;

int main(void)
{
    int rc;

    printf("=== tstasmrc: asm stores the compiler is told about (#425 #427) ===\n");

    CHECK(ms_readback() == 0,   "memset: a value stored before is gone after");
    CHECK(ms_char() == 'B',     "memset: the fill character is read back");
    CHECK(ms_loop(three) == 0,  "memset: a store before it is not sunk past it");
    CHECK(mc_readback() == 0,   "memclr: a value stored before is gone after");

    rc = clib_identify_cthread();       /* CTHREAD is linked: the startup */
    printf("  clib_identify_cthread() = %d\n", rc);   /* IDENTIFYed it already */
    CHECK(rc == 4, "clib_identify_cthread() returns IDENTIFY's RC (4: exists)");

    printf("=== tstasmrc: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0)
        printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return (mbt_failed > 0 ? 1 : 0);
}
