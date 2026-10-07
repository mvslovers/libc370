/*
 * tstrseek.c - libc370 #473 on MVS: fseek() inside the buffer of an "r"
 * stream.
 *
 * After one FB 80 record of a text stream was read, fseek(fp, 0, SEEK_SET)
 * returned 0 but left ftell() at 81, and the next read came from the 81
 * bytes in front of the stream's buffer: __fseek() took its in-buffer
 * shortcut with the offset counted from the current position instead of
 * the buffer's start, and did not move filepos.  brexx370's LINEIN(file, n)
 * rewinds that way and returned the wrong line once its module grew.
 *
 * This program writes three FB 80 records whose bytes name their position
 * (byte 0 the record's letter, byte i the digit i % 10) and reads them
 * back in text mode:
 *
 *   SEEK0   fgets() line 1, fseek(0, SEEK_SET): ftell() 0, then fgets()
 *           lines 1 and 2 again                                     (#473)
 *   TELL    fgetc() twice, fseek(ftell(), SEEK_SET): ftell() 2, next '2'
 *   BACK1   fgetc() three times, fseek(-1, SEEK_CUR): ftell() 2, next '2'
 *   FWD     fgetc() once, fseek(10, SEEK_SET): ftell() 10, next '0'
 *   REOPEN  line 1 and one byte of line 2, fseek(0): next 'A'  (unchanged)
 *
 * Built twice from this source: TSTRSK against this tree's libc.a, TSTRSKR
 * against the installed one, the red control (the first four fail there).
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstrseek.c -o TSTRSK -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude \
 *                test/mvs/tstrseek.c -o TSTRSKR -flinker-output=iebcopy
 *          ld370 --pack TSTRSK=TSTRSK.iebcopy TSTRSKR=TSTRSKR.iebcopy \
 *                -o tstrseek -xmit --dsn IBMUSER.LIBC370.RSKSCR
 * Install: jcl/recvrsk.jcl.   Run: jcl/tstrseek.jcl.
 *
 * mvsdev JOB01675, 2026-10-07 (RECEIVE JOB01674): GREEN CC 0000, 8/8;
 * RED (installed 2.6.1) CC 0001, 3/8: SEEK0 ftell 81 after the seek and
 * lines ' ' '6', TELL next 'A', BACK1 ftell 3, FWD ftell 1 next '9'.
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

static const char digits[] = "0123456789";

static FILE *openr(void)
{
    FILE *fp = fopen("DD:TXT", "r");
    if (!fp) printf("  open DD:TXT for read failed\n");
    return fp;
}

int main(void)
{
    FILE    *fp;
    char    rec[LRECL];
    char    line[LRECL + 8];
    char    l1, l2;
    int     i, c, rc;
    long    pos, pos0;

    printf("=== tstrseek: fseek() inside the buffer of \"r\" (#473) ===\n\n");

    fp = fopen("DD:TXT", "w");
    CHECK(fp != NULL, "open DD:TXT for write");
    if (!fp) goto done;
    for (i = 0; i < 3; i++) {
        int j;
        rec[0] = (char)("ABC"[i]);
        for (j = 1; j < LRECL; j++) rec[j] = digits[j % 10];
        fwrite(rec, 1, LRECL, fp);
        fputc('\n', fp);
    }
    CHECK(fclose(fp) == 0, "close after write");

    /* SEEK0: brexx370's LINEIN(file, 3) after LINEIN(file, 1) */
    if (!(fp = openr())) goto done;
    fgets(line, sizeof(line), fp);
    pos  = ftell(fp);
    rc   = fseek(fp, 0, SEEK_SET);
    pos0 = ftell(fp);
    l1   = fgets(line, sizeof(line), fp) ? line[0] : '?';
    l2   = fgets(line, sizeof(line), fp) ? line[0] : '?';
    printf("  SEEK0  ftell %ld, fseek(0) rc %d, ftell %ld, lines '%c' '%c',"
           " ftell %ld\n", pos, rc, pos0, l1, l2, ftell(fp));
    CHECK(pos == 81 && rc == 0 && pos0 == 0,
          "SEEK0 fseek(0) after one record: ftell 0");
    CHECK(l1 == 'A' && l2 == 'B' && ftell(fp) == 162,
          "SEEK0 ...then lines 1 and 2, ftell 162");
    fclose(fp);

    /* TELL: fseek(ftell()) mid-record stays */
    if (!(fp = openr())) goto done;
    fgetc(fp); fgetc(fp);
    rc  = fseek(fp, ftell(fp), SEEK_SET);
    pos = ftell(fp);
    c   = fgetc(fp);
    printf("  TELL   rc %d, ftell %ld, next '%c'\n", rc, pos, c);
    CHECK(rc == 0 && pos == 2 && c == '2',
          "TELL fseek(ftell()) mid-record: ftell 2, next '2'");
    fclose(fp);

    /* BACK1: one byte back */
    if (!(fp = openr())) goto done;
    fgetc(fp); fgetc(fp); fgetc(fp);
    rc  = fseek(fp, -1, SEEK_CUR);
    pos = ftell(fp);
    c   = fgetc(fp);
    printf("  BACK1  rc %d, ftell %ld, next '%c'\n", rc, pos, c);
    CHECK(rc == 0 && pos == 2 && c == '2',
          "BACK1 fseek(-1, SEEK_CUR): ftell 2, next '2'");
    fclose(fp);

    /* FWD: forward inside the buffer */
    if (!(fp = openr())) goto done;
    fgetc(fp);
    rc  = fseek(fp, 10, SEEK_SET);
    pos = ftell(fp);
    c   = fgetc(fp);
    printf("  FWD    rc %d, ftell %ld, next '%c'\n", rc, pos, c);
    CHECK(rc == 0 && pos == 10 && c == '0',
          "FWD fseek(10) in the buffer: ftell 10, next '0'");
    fclose(fp);

    /* REOPEN: a target in front of the buffer */
    if (!(fp = openr())) goto done;
    fgets(line, sizeof(line), fp);
    fgetc(fp);
    rc  = fseek(fp, 0, SEEK_SET);
    pos = ftell(fp);
    c   = fgetc(fp);
    printf("  REOPEN rc %d, ftell %ld, next '%c'\n", rc, pos, c);
    CHECK(rc == 0 && pos == 0 && c == 'A',
          "REOPEN fseek(0) in front of the buffer: ftell 0, next 'A'");
    fclose(fp);

done:
    printf("\n=== tstrseek: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
