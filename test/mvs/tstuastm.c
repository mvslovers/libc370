/*
 * tstuastm.c - libc370 #125 on MVS: __uastep() keeps its CRT pointer
 * across TESTAUTH.  (APF-AUTHORIZED, run from SYS2.LINKLIB.)
 *
 * The TESTAUTH in __uastep() declared no clobbers but "memory".  cc370
 * kept the CRT pointer __crtget() returned in R15, and TESTAUTH sets R15
 * to its return code: 0 for an authorized caller.  The test that follows,
 * "if (crt) { if (!(crt->crtauth & CRTAUTH_STEPLIB)) goto quit; }", then
 * saw no CRT, and __uastep() turned off the APF bit of the STEPLIB DEB
 * although __austep() had never turned it on.
 *
 *   1 the step runs authorized, with SYS2.LINKLIB (APF) as STEPLIB
 *   2 __uastep() without __austep() before it leaves the STEPLIB DEB's
 *     APF bit (DEBAPFIN) as it was: on
 *
 * BUILD (from the repository root, after make; S a scratch directory),
 * AC=1 on the link and again on the pack:
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstuastm.c \
 *         -o $S/TSTUASTG -flinker-output=iebcopy -Wl,--ac,1
 *     cc370 -O1 -Wall -Werror test/mvs/tstuastm.c \
 *         -o $S/TSTUASTR -flinker-output=iebcopy -Wl,--ac,1
 *     cd $S && ld370 --pack --ac 1 TSTUASTG=TSTUASTG.iebcopy \
 *         TSTUASTR=TSTUASTR.iebcopy -o ua -xmit --dsn IBMUSER.LIBC370.UASCR
 *
 * then jcl/recvuastm.jcl and jcl/tstuastm.jcl, which copies both into
 * SYS2.LINKLIB - in a disposable container only.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <mvs/wto.h>
#include <mvs/apf.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTUASTM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* the APF bit of the STEPLIB DEB: 1 on, 0 off, -1 no STEPLIB */
__attribute__((noinline))
static int apfbit(void)
{
    int r;

    __asm__("L     %0,=F'-1'\n"
"         ICM   1,15,PSATOLD-PSA(0) our TCB\n"
"         ICM   1,15,TCBJLB-TCB(1)  STEPLIB DCB\n"
"         LA    1,0(,1)\n"
"         LTR   1,1\n"
"         BZ    APFBITX\n"
"         USING IHADCB,1\n"
"         L     1,DCBDEBAD          its DEB\n"
"         N     1,=X'00FFFFFF'\n"
"         USING DEBBASIC,1\n"
"         SR    %0,%0\n"
"         TM    DEBFLGS1,DEBAPFIN\n"
"         BZ    APFBITX\n"
"         LA    %0,1\n"
"         DROP  1\n"
"APFBITX  DS    0H" : "=r"(r) : : "1", "memory");
    return r;
}

int main(void)
{
    int before, after;

    want(__isauth(), "the step is APF authorized");
    before = apfbit();
    __uastep();
    after = apfbit();
    wtof("TSTUASTM   STEPLIB DEB APF bit: %d before __uastep(), %d after",
         before, after);
    want(before == 1 && after == 1,
         "__uastep() without __austep() leaves the STEPLIB APF bit");
    return bad ? 8 : 0;
}

__asm__("PRINT NOGEN");
__asm__("IHAPSA ,");
__asm__("IKJTCB DSECT=YES");
__asm__("DCBD DSORG=PO,DEVD=DA");
__asm__("IEZDEB");
__asm__("PRINT GEN");
__asm__("CSECT");
