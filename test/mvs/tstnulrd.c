/*
 * tstnulrd.c - libc370 #454 on MVS: a text-mode read keeps a X'00' inside
 * a record and drops only the trailing ones.
 *
 * __fgetc() looked for "trailing NULs" with memchr(), which finds the
 * first X'00' anywhere, so a text read cut the record there and lost the
 * rest.  This program writes four FB 80 records through a binary stream
 * (full records, so nothing is padded) and reads them back in text mode:
 *
 *   R1  'A' X'00' 'B' + 77 blanks   all 80 bytes        (was 1: "A")
 *   R2  "CD" + 78 X'00'             "CD", 2 bytes       (unchanged)
 *   R3  X'00' 'E' + 78 X'00'        X'00' 'E', 2 bytes  (was 0)
 *   R4  80 X'00'                    an empty line       (unchanged)
 *
 * and once in binary mode, which never stripped anything (80 bytes each).
 *
 * Built twice from this source: TSTNRD against this tree's libc.a, TSTNRDR
 * against the installed one, the red control (R1 and R3 fail there).
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstnulrd.c -o TSTNRD -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude \
 *                test/mvs/tstnulrd.c -o TSTNRDR -flinker-output=iebcopy
 *          ld370 --pack TSTNRD=TSTNRD.iebcopy TSTNRDR=TSTNRDR.iebcopy \
 *                -o tstnulrd -xmit --dsn IBMUSER.LIBC370.NRDSCR
 * Install: jcl/recvnrd.jcl.   Run: jcl/tstnulrd.jcl.
 *
 * mvsdev JOB01542, 2026-10-06 (RECEIVE JOB01541): GREEN CC 0000, 13/13;
 * RED (installed 2.4.0) CC 0001, R1 read as 1 byte and R3 as 0.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>

#define LRECL 80

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

static unsigned char rec[4][LRECL];

static void build(void)
{
    memset(rec, 0, sizeof(rec));
    rec[0][0] = 'A';
    rec[0][2] = 'B';
    memset(&rec[0][3], ' ', LRECL - 3);
    rec[1][0] = 'C';
    rec[1][1] = 'D';
    rec[2][1] = 'E';
}

/* one text line into buf (without the '\n'); -> its length, -1 at EOF */
static int getline_text(FILE *fp, unsigned char *buf, int max)
{
    int c, n = 0;

    while ((c = fgetc(fp)) != EOF && c != '\n') {
        if (n < max) buf[n] = (unsigned char)c;
        n++;
    }
    if (c == EOF && n == 0) return -1;
    return n;
}

int main(void)
{
    FILE            *fp;
    unsigned char   buf[LRECL + 8];
    int             i, n[4];
    size_t          got;
    char            msg[80];

    printf("=== tstnulrd: X'00' in a text-mode read (#454) ===\n\n");
    build();

    fp = fopen("DD:TXT", "wb");
    CHECK(fp != NULL, "open DD:TXT for binary write");
    if (!fp) goto done;
    for (i = 0; i < 4; i++) fwrite(rec[i], 1, LRECL, fp);
    CHECK(fclose(fp) == 0, "close after write");

    fp = fopen("DD:TXT", "r");
    CHECK(fp != NULL, "open DD:TXT for text read");
    if (!fp) goto done;
    for (i = 0; i < 4; i++) {
        memset(buf, 0xFF, sizeof(buf));
        n[i] = getline_text(fp, buf, LRECL);
        printf("  R%d: %d bytes\n", i + 1, n[i]);
        switch (i) {
        case 0:
            CHECK(n[0] == LRECL && memcmp(buf, rec[0], LRECL) == 0,
                  "R1 'A' X'00' 'B' + blanks: all 80 bytes");
            break;
        case 1:
            CHECK(n[1] == 2 && buf[0] == 'C' && buf[1] == 'D',
                  "R2 \"CD\" + X'00's: \"CD\", trailing X'00's dropped");
            break;
        case 2:
            CHECK(n[2] == 2 && buf[0] == 0 && buf[1] == 'E',
                  "R3 X'00' 'E' + X'00's: X'00' 'E'");
            break;
        case 3:
            CHECK(n[3] == 0, "R4 all X'00': an empty line");
            break;
        }
    }
    CHECK(getline_text(fp, buf, LRECL) == -1, "end of file after R4");
    fclose(fp);

    fp = fopen("DD:TXT", "rb");
    CHECK(fp != NULL, "open DD:TXT for binary read");
    if (!fp) goto done;
    for (i = 0; i < 4; i++) {
        got = fread(buf, 1, LRECL, fp);
        sprintf(msg, "binary R%d: 80 bytes as written", i + 1);
        CHECK(got == LRECL && memcmp(buf, rec[i], LRECL) == 0, msg);
    }
    fclose(fp);

done:
    printf("\n=== tstnulrd: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
