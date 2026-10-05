/*
 * tstpremn.c - libc370 cc370#10 on MVS: the __premain() startup hook.
 *
 * A program that defines __premain() gets it called by libc370's startup
 * before the standard streams are opened and before main().  A stream it
 * sets is kept, one it leaves NULL is opened as usual, and a nonzero
 * return ends the program with that code without main().  A program that
 * does not define it runs exactly as before.
 *
 * Built four ways from this source:
 *   TSTPRMW  -DHOOK_OK, startup compiled by a cc370 with weak references
 *            (__CC370_WEAK__, cc370 1.3 and later)
 *   TSTPRMF  -DHOOK_OK, startup compiled by cc370 1.1.0: the inline-asm
 *            WXTRN fallback (@@start.o linked explicitly)
 *   TSTPRMR  -DHOOK_RC: the hook returns 12; main() must not run
 *   TSTPRMN  no hook: the control
 * HOOK_OK points stdout at DD MYOUT; jcl/tstpremn.jcl checks that main()'s
 * line lands there.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -DHOOK_OK -c \
 *                test/mvs/tstpremn.c -o ok.o          (likewise -DHOOK_RC,
 *                and none, for rc.o and no.o)
 *          ld370 --entry @@CRT0 ok.o -L build/sdk -lc -L <sysroot>/lib \
 *                -lcc370rt -iebcopy -o TSTPRMW           (rc.o, no.o alike)
 *          <cc370 1.1.0> -c -Os -std=gnu99 -trigraphs -Iinclude -I. \
 *                src/mvs/crt/@@start.c -o start11.o
 *          ld370 --entry @@CRT0 ok.o start11.o -L build/sdk -lc ... \
 *                -iebcopy -o TSTPRMF
 *          ld370 --pack TSTPRMW=... TSTPRMF=... TSTPRMR=... TSTPRMN=... \
 *                -o tstpremn -xmit --dsn IBMUSER.LIBC370.PRMSCR
 * Install: jcl/recvprm.jcl.   Run: jcl/tstpremn.jcl.
 *
 * mvsdev JOB01398, 2026-10-05 (RECEIVE JOB01397): W and F CC 0000 with
 * main()'s line in MYOUT and no SYSPRINT opened; R CC 0012 and no output;
 * N CC 0000.
 *
 * RC: 0 = every check passed, 1 = a check failed, 12 = TSTPRMR's hook.
 */
#include <stdio.h>
#include <string.h>
#include <mvs/crt.h>

#if defined(HOOK_OK) || defined(HOOK_RC)
static int  hook_ran;
static char hook_pgm[9];

int __premain(char *parm, char *pgmname, void **pgmr1)
{
    (void)parm;
    (void)pgmr1;
    hook_ran = 1;
    memcpy(hook_pgm, pgmname, 8);
#ifdef HOOK_RC
    return 12;                       /* main() must not run */
#else
    stdout = fopen("DD:MYOUT", "w"); /* the startup keeps this one */
    return 0;
#endif
}
#endif

int main(void)
{
    int failed = 0;

#if defined(HOOK_OK)
    printf("main: hook_ran=%d pgm=%.8s (this line belongs in MYOUT)\n",
           hook_ran, hook_pgm);
    if (!hook_ran) failed++;
    if (memcmp(hook_pgm, "TSTPRM", 6) != 0) failed++;
    fprintf(stderr, "stderr: opened by the startup as usual\n");
#elif defined(HOOK_RC)
    printf("main: ran although the hook returned 12\n");
    failed++;
#else
    printf("main: no hook, startup as before\n");
#endif
    printf("%s\n", failed ? "FAIL" : "PASS");
    return failed ? 1 : 0;
}
