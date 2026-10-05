/*
 * tstvsrd.c - libc370 #426/#411 on MVS: vsread() reports end of file on
 * the read that reaches it, and only there.
 *
 * ISSUE #426: the EODAD and SYNAD exits set vs->flags while the GET runs,
 * and the GET asm did not say so.  Built with -Os (2.2.0 and later), the
 * library tested a copy of the flags taken before the GET: the read that
 * hit the end returned a record length, and only the next one -1.
 * ISSUE #411: the flags stayed set, so after one error every later read
 * failed until vsclear().  vsread() now returns what its own GET did;
 * vseof()/vserror() still report the sticky state, like feof()/ferror().
 *
 * DD VSAMESDS is a two-record ESDS that jcl/tstvsrd.jcl defines, loads
 * and deletes again.  Built twice: TSTVSRD against this tree's libc.a,
 * TSTVSRDR against the installed sysroot libc.a (2.2.0, -Os) - the red
 * control.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Iinclude -L build/sdk \
 *                test/mvs/tstvsrd.c -o TSTVSRD -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Iinclude \
 *                test/mvs/tstvsrd.c -o TSTVSRDR -flinker-output=iebcopy
 *          ld370 --pack TSTVSRD=TSTVSRD.iebcopy TSTVSRDR=TSTVSRDR.iebcopy \
 *                -o tstvsrd -xmit --dsn IBMUSER.LIBC370.VSRSCR
 * Install: jcl/recvvsr.jcl.   Run: jcl/tstvsrd.jcl.
 *
 * mvsdev JOB01378, 2026-10-05 (RECEIVE JOB01377): GREEN CC 0000, 7/7;
 * RED (installed 2.2.0) CC 0001, 6/7: read 3 returned 80, read 4 -1.
 * A GET after the end is a VSAM logical error with feedback X'04'
 * (measured, JOB01376); vsread() reports it as end of file.
 *
 * (mvs/vsam.h fails -Werror on #pragma pack, #415, so no -Werror here.)
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <mvs/vsam.h>

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

int main(void)
{
    VSFILE  *vs = NULL;
    char    buf[81];
    int     rc;
    int     r;

    printf("=== tstvsrd: vsread() and end of file (#426 #411) ===\n");

    rc = vsopen("VSAMESDS", VSTYPE_ESDS, VSACCESS_SEQ, VSMODE_IN, &vs);
    CHECK(rc == 0 && vs != NULL, "vsopen() of the two-record ESDS");
    if (rc != 0 || vs == NULL) {
        printf("  vsopen rc=%d\n", rc);
        goto done;
    }

    memset(buf, 0, sizeof buf);
    r = vsread(vs, buf, 80, NULL, 0);
    printf("  read 1: %d \"%.8s\"\n", r, buf);
    CHECK(r == 80 && memcmp(buf, "RECORD1 ", 8) == 0, "read 1 is RECORD1");

    memset(buf, 0, sizeof buf);
    r = vsread(vs, buf, 80, NULL, 0);
    printf("  read 2: %d \"%.8s\"\n", r, buf);
    CHECK(r == 80 && memcmp(buf, "RECORD2 ", 8) == 0, "read 2 is RECORD2");

    memset(buf, 0, sizeof buf);
    r = vsread(vs, buf, 80, NULL, 0);
    printf("  read 3: %d\n", r);
    CHECK(r == -1, "read 3 reports end of file, not a record length");
    CHECK(vseof(vs) == 1, "vseof() says end of file");

    r = vsread(vs, buf, 80, NULL, 0);
    printf("  read 4: %d (VSAM rc %u, reason %u)\n", r, vs->rc, vs->rsn);
    CHECK(r == -1, "read 4 still reports end of file");

    vsclear(vs);
    CHECK(vseof(vs) == 0, "vsclear() resets the end-of-file flag");

    vsclose(vs);

done:
    printf("=== tstvsrd: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0)
        printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return (mbt_failed > 0 ? 1 : 0);
}
