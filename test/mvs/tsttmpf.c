/*
 * tsttmpf.c - libc370 #395 item 3 on MVS: tmpfile() reads back what was
 * written to it.
 *
 * tmpfile() opened its temporary data set "wb", so after rewind() fread()
 * returned 0.  C99 7.19.4.3 asks for "wb+".  The cases:
 *
 *   SMALL  11 bytes written, rewind(), 11 read back
 *   BIG    20000 bytes (many blocks) written, rewind(), all read back
 *   TWO    two tmpfile() streams at once, each reads back its own data
 *
 * Built twice from this source: TSTTMP against this tree's libc.a, TSTTMPR
 * against the installed one, the red control.  No DD is needed:
 * tmpfile() allocates a &&TMP data set itself.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tsttmpf.c -o TSTTMP -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude \
 *                test/mvs/tsttmpf.c -o TSTTMPR -flinker-output=iebcopy
 *          ld370 --pack TSTTMP=TSTTMP.iebcopy TSTTMPR=TSTTMPR.iebcopy \
 *                -o tsttmpf -xmit --dsn IBMUSER.LIBC370.TMPSCR
 * Install: jcl/recvtmp.jcl.   Run: jcl/tsttmpf.jcl.
 *
 * mvsdev JOB01589, 2026-10-07 (RECEIVE JOB01588): GREEN CC 0000, 7/7;
 * RED (installed 2.4.1) CC 0001, 3/7: every read after rewind() got 0.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>

#define BIG 20000

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

static char data[BIG];
static char back[BIG + 64];

int main(void)
{
    FILE    *fp, *f2;
    size_t  n, n2;
    long    i;

    printf("=== tsttmpf: tmpfile() reads back (#395) ===\n\n");

    fp = tmpfile();
    CHECK(fp != NULL, "SMALL: tmpfile()");
    if (fp) {
        n = fwrite("hello world", 1, 11, fp);
        rewind(fp);
        n2 = fread(back, 1, 64, fp);
        printf("  SMALL: wrote %lu, read %lu\n", (unsigned long)n,
               (unsigned long)n2);
        CHECK(n == 11 && n2 == 11 && memcmp(back, "hello world", 11) == 0,
              "SMALL: 11 bytes read back");
        fclose(fp);
    }

    for (i = 0; i < BIG; i++) data[i] = (char)('A' + i % 26);
    fp = tmpfile();
    CHECK(fp != NULL, "BIG: tmpfile()");
    if (fp) {
        n = fwrite(data, 1, BIG, fp);
        rewind(fp);
        n2 = fread(back, 1, sizeof(back), fp);
        printf("  BIG: wrote %lu, read %lu\n", (unsigned long)n,
               (unsigned long)n2);
        CHECK(n == BIG && n2 >= BIG && memcmp(back, data, BIG) == 0,
              "BIG: 20000 bytes read back");
        fclose(fp);
    }

    fp = tmpfile();
    f2 = tmpfile();
    CHECK(fp != NULL && f2 != NULL, "TWO: two tmpfile() streams");
    if (fp && f2) {
        fwrite("first", 1, 5, fp);
        fwrite("second", 1, 6, f2);
        rewind(fp);
        rewind(f2);
        n = fread(back, 1, 64, fp);
        CHECK(n == 5 && memcmp(back, "first", 5) == 0, "TWO: the first has its own");
        n = fread(back, 1, 64, f2);
        CHECK(n == 6 && memcmp(back, "second", 6) == 0, "TWO: the second has its own");
    }
    if (fp) fclose(fp);
    if (f2) fclose(f2);

    printf("\n=== tsttmpf: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
