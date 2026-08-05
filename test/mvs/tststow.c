/*
 * tststow.c - libc370 #32 regression probe (MVS target, batch).
 *
 * ISSUE #32: __stow() emitted no instruction at all.  as370 has no STOW
 * opcode and there is no stow macro in the macro library, so the mnemonic
 * assembled to nothing, the func letter was left untouched in R15, and
 * __stow() returned it as a return code (rc=195 = X'C3' = 'C').  Every PDS
 * directory operation was a silent no-op.
 *
 * The fix builds the SVC 21 linkage by hand, transcribed from
 * SYS1.MACLIB(STOW) + IHBINNRA: R1 = DCB, R0 = area, and the function is
 * encoded by NEGATING those registers (LCR), not by a function code byte.
 * Getting that encoding wrong is easy and would not show up at build time,
 * so it has to be proven on a real PDS.
 *
 * A test that only checks "rc == 0" would be satisfied by a STOW that never
 * ran.  This one forces THREE different STOW return codes out of the same
 * data set, which only a real SVC 21 can produce:
 *
 *   1. rename M1 -> M2      expect rc=0    the rename works
 *   2. rename M1 -> M3      expect rc=8    old name not found - proving M1
 *                                          really is gone from the directory
 *   3. rename M2 -> M9      expect rc=4    new name already exists - proving
 *                                          STOW inspects the directory
 *
 * Before the fix all three returned 195.
 *
 * SETUP - the PDS must exist with members M1 and M9 (any content):
 *     zowe files create pds "<hlq>.STOWTEST.PDS" --record-format FB \
 *          --record-length 80 --block-size 3120 --size 1CYL
 *     echo hello | zowe files upload stds - "<hlq>.STOWTEST.PDS(M1)"
 *     echo hello | zowe files upload stds - "<hlq>.STOWTEST.PDS(M9)"
 *
 * PARM='<dsn>'   (default IBMUSER.STOWTEST.PDS)
 *
 * BUILD (host):
 *     cc370 -Iinclude test/mvs/tststow.c -flinker-output=iebcopy -o TSTSTOW
 *     ld370 --pack TSTSTOW.iebcopy -o probe -xmit --dsn <LOADLIB>
 *
 * RUN: see jcl/tststow.jcl.  Built by hand - libc370 is the cc370 sysroot,
 * not an mbt project, so nothing compiles this translation unit automatically.
 *
 * RC: 0 = all three expectations met, 8 = at least one did not.
 */
#include <stdio.h>
#include <string.h>
#include "clibio.h"     /* __renmem */

static int check(const char *what, int got, int want);

int main(int argc, char **argv)
{
    const char *dsn = "IBMUSER.STOWTEST.PDS";
    char        buf[45];
    int         bad = 0;
    int         rc;

    if (argc > 1 && argv[1] && argv[1][0] > ' ') {
        unsigned i;

        for (i = 0; i < sizeof(buf) - 1 && argv[1][i] > ' '; i++) {
            buf[i] = argv[1][i];
        }
        buf[i] = 0;
        dsn = buf;
    }

    printf("TSTSTOW - libc370 #32 probe, dsn '%s'\n\n", dsn);

    /* 1. the rename itself */
    rc = __renmem(dsn, "M1", "M2");
    bad += check("rename M1 -> M2 (expect success)", rc, 0);

    /* 2. M1 must now be gone from the directory.  Before the fix this also
          returned 195, i.e. it could not tell us anything. */
    rc = __renmem(dsn, "M1", "M3");
    bad += check("rename M1 -> M3 (expect 8, old name not found)", rc, 8);

    /* 3. and STOW must see that M9 is already there */
    rc = __renmem(dsn, "M2", "M9");
    bad += check("rename M2 -> M9 (expect 4, new name exists)", rc, 4);

    printf("\nTSTSTOW %s\n", bad ? "FAILED" : "PASSED");
    printf("  (a returned 195 = X'C3' = 'C' means STOW emitted no code - #32)\n");

    return bad ? 8 : 0;
}

static int check(const char *what, int got, int want)
{
    int bad = (got != want);

    printf("  %-46s rc=%-5d %s\n", what, got, bad ? "*** FAIL" : "ok");
    if (bad) {
        printf("  %-46s expected rc=%d\n", "", want);
    }
    return bad;
}
