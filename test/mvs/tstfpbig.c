/*
 * tstfpbig.c - libc370 #385 on MVS: fprintf() of 8192 characters and more.
 *
 * fprintf() formatted into an 8 KB stack buffer and fwrite() that, so a
 * result of 8192 characters or more was cut to 8192 bytes with X'00' as
 * the last one, and the return value was fwrite()'s count.  printf() and
 * vfprintf() never had the limit.  Now fprintf() goes through vfprintf()
 * too, and an output error returns a negative value.
 *
 * Each case writes through a binary stream to its own FB 80 data set,
 * reads it back in binary, and checks the return value and the bytes:
 *
 *   BIG1  "%s" of 9000 'x'          ret 9000, 9000 'x'   (was 8192, X'00')
 *   BIG2  "%s" of 8192 'x'          ret 8192, 8192 'x'   (last was X'00')
 *   BIG3  "%*d", 9000, 7            ret 9000, 8999 blanks and '7'
 *   BIG1  fprintf() to it opened "rb"  ret < 0         (was 0)
 *
 * Built twice from this source: TSTFPB against this tree's libc.a, TSTFPBR
 * against the installed one, the red control.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstfpbig.c -o TSTFPB -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude \
 *                test/mvs/tstfpbig.c -o TSTFPBR -flinker-output=iebcopy
 *          ld370 --pack TSTFPB=TSTFPB.iebcopy TSTFPBR=TSTFPBR.iebcopy \
 *                -o tstfpbig -xmit --dsn IBMUSER.LIBC370.FPBSCR
 * Install: jcl/recvfpb.jcl.   Run: jcl/tstfpbig.jcl.
 *
 * mvsdev JOB01544, 2026-10-06 (RECEIVE JOB01543): GREEN CC 0000, 8/8;
 * RED (installed 2.4.1) CC 0001, 2/8: every case cut at 8192 with the
 * last byte X'00' (8191 data bytes), ret 8192, and ret 0 on a read stream.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

static char text[9001];
static char back[9200];

/* the data set's bytes, read in binary; -> how many before the X'00'
   padding of the last record */
static long readback(const char *dd)
{
    FILE    *fp = fopen(dd, "rb");
    size_t  n;

    if (!fp) return -1;
    n = fread(back, 1, sizeof(back), fp);
    fclose(fp);
    while (n > 0 && back[n - 1] == '\0') n--;
    return (long)n;
}

static int all(const char *p, long n, char c)
{
    long i;

    for (i = 0; i < n; i++) if (p[i] != c) return 0;
    return 1;
}

static int writeone(const char *dd, const char *fmt, int width, int value,
                    const char *s)
{
    FILE    *fp = fopen(dd, "wb");
    int     ret;

    if (!fp) return -2;
    ret = s ? fprintf(fp, fmt, s) : fprintf(fp, fmt, width, value);
    fclose(fp);
    return ret;
}

int main(void)
{
    FILE    *fp;
    int     ret;
    long    n;

    printf("=== tstfpbig: fprintf() of 8192 characters and more (#385) ===\n\n");

    memset(text, 'x', 9000);
    text[9000] = '\0';
    ret = writeone("DD:BIG1", "%s", 0, 0, text);
    n = readback("DD:BIG1");
    printf("  BIG1: ret %d, %ld bytes\n", ret, n);
    CHECK(ret == 9000, "BIG1 9000 characters: returns 9000");
    CHECK(n == 9000 && all(back, 9000, 'x'), "BIG1: 9000 'x' written");

    text[8192] = '\0';
    ret = writeone("DD:BIG2", "%s", 0, 0, text);
    n = readback("DD:BIG2");
    printf("  BIG2: ret %d, %ld bytes\n", ret, n);
    CHECK(ret == 8192, "BIG2 8192 characters: returns 8192");
    CHECK(n == 8192 && all(back, 8192, 'x'), "BIG2: 8192 'x', the last one too");

    ret = writeone("DD:BIG3", "%*d", 9000, 7, NULL);
    n = readback("DD:BIG3");
    printf("  BIG3: ret %d, %ld bytes\n", ret, n);
    CHECK(ret == 9000, "BIG3 %*d of width 9000: returns 9000");
    CHECK(n == 9000 && all(back, 8999, ' ') && back[8999] == '7',
          "BIG3: 8999 blanks and '7'");

    fp = fopen("DD:BIG1", "rb");
    CHECK(fp != NULL, "open DD:BIG1 for reading");
    if (fp) {
        ret = fprintf(fp, "x");
        printf("  fprintf to a read stream: ret %d\n", ret);
        CHECK(ret < 0, "fprintf to a stream not open for writing: negative");
        fclose(fp);
    }

    printf("\n=== tstfpbig: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
