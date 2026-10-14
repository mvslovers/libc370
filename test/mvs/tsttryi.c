/*
 * tsttryi.c - libc370 #530 on MVS: after try() catches an abend,
 * tryinfo() says where it happened.
 *
 *   (t1) a function that executes DC H'0' at a known label abends S0C1;
 *        tryrc() is 0x0C1000
 *   (t2) the PSW address is that label + 2, the ILC 1 (one halfword),
 *        the interruption code 0001 (operation)
 *   (t3) R14 is set (the return address into call(), try()'s static
 *        helper that made the BALR); its distance from ___try is
 *        printed, not checked - that is code layout, not a guarantee
 *   (t4) a following try() of a function that completes clears all of
 *        it: tryrc() 0, PSW and R14 zero
 *
 * Each check prints PASS or FAIL with the values; RC 0 = all passed,
 * 8 = at least one failed.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tsttryi.c -o TSTTRYI -flinker-output=iebcopy
 *          ld370 --pack TSTTRYI=TSTTRYI.iebcopy \
 *                -o tsttryi -xmit --dsn IBMUSER.LIBC370.TRYISCR
 * Upload:  tsttryi.xmit to IBMUSER.LIBC370.TRYIXMIT (FB 80, binary)
 * Install: jcl/recvtryi.jcl.   Run: jcl/tsttryi.jcl.
 */
#include <stdio.h>
#include <mvs/recovery.h>

static int failures;

static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

/* abends S0C1 at BOOMAT: an invalid operation code */
__asm__("\n&FUNC    SETC 'boom'");
static int boom(void)
{
    __asm__("\nBOOMAT   DS    0H\n"
            "         DC    H'0'");
    return 0;
}

__asm__("\n&FUNC    SETC 'calm'");
static int calm(void)
{
    return 0;
}

int main(void)
{
    const TRYINFO   *ti;
    unsigned        rc;
    unsigned        at;
    unsigned        addr;
    unsigned        ilc;
    unsigned        intc;
    unsigned        r14;
    unsigned        base = (unsigned)___try & 0x00FFFFFF;

    __asm__("L\t%0,=A(BOOMAT)" : "=r" (at));
    at &= 0x00FFFFFF;

    try(boom, 0);
    rc = tryrc();
    ti = tryinfo();
    addr = ti->psw[1] & 0x00FFFFFF;
    ilc  = (ti->psw[1] >> 30) & 3;
    intc = ti->psw[0] & 0xFFFF;
    r14  = ti->r14 & 0x00FFFFFF;

    printf("BOOMAT=%06X TRYRC=%06X PSW=%08X %08X R14=%08X ___TRY=%06X\n",
           at, rc, ti->psw[0], ti->psw[1], ti->r14, base);

    check(rc == 0x0C1000, "(t1) tryrc() is S0C1");
    check(addr == at + 2, "(t2) PSW address is BOOMAT+2");
    check(ilc == 1, "(t2) ILC is 1");
    check(intc == 1, "(t2) interruption code is 0001");
    check(r14 != 0, "(t3) R14 is set");
    printf("R14 is ___TRY%+d (call() precedes ___try in its CSECT)\n",
           (int)r14 - (int)base);

    try(calm, 0);
    rc = tryrc();
    ti = tryinfo();
    printf("TRYRC=%06X PSW=%08X %08X R14=%08X\n",
           rc, ti->psw[0], ti->psw[1], ti->r14);
    check(rc == 0 && ti->psw[0] == 0 && ti->psw[1] == 0 && ti->r14 == 0,
          "(t4) a clean try() clears tryrc() and tryinfo()");

    printf("%s\n", failures ? "TSTTRYI FAILED" : "TSTTRYI PASSED");
    return failures ? 8 : 0;
}
