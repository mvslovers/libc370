/*
 * tstfskm.c - libc370 #178 on MVS: fseek() keeps the error indicator and
 * clears only the end-of-file indicator; rewind() clears both.
 *
 * C99 7.19.9.2 and 7.19.9.5.  The error indicator is set two ways the
 * probe controls:
 *
 *   (a) a read stream: fseek() beyond the end of the data set fails and
 *       sets the error and end-of-file indicators.  A successful fseek()
 *       back to 0 then clears end-of-file and keeps the error; rewind()
 *       clears both and the stream reads again.
 *   (b) fgetc() to the end sets end-of-file alone; fseek() clears it.
 *   (c) a write stream runs out of space (DD FULL, one track, no
 *       secondary).  fseek() to where it is succeeds and the error stays,
 *       so the next write still fails.
 *
 * DD WORK: a scratch sequential data set, FB 80.  DD FULL: the same on
 * SPACE=(TRK,(1,0)).
 *
 * BUILD (from the repository root, after make; S a scratch directory,
 * MAIN a build of origin/main for the red control):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstfskm.c \
 *         -o $S/TSTFSKG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror -I$MAIN/include -L $MAIN/build/sdk \
 *         test/mvs/tstfskm.c -o $S/TSTFSKR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTFSKG=TSTFSKG.iebcopy \
 *         TSTFSKR=TSTFSKR.iebcopy -o fsk -xmit --dsn IBMUSER.LIBC370.FSKSCR
 *
 * then jcl/recvfskm.jcl and jcl/tstfskm.jcl.
 *
 * RC 0 = passed, 8 = failed, 12 = could not measure.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <mvs/wto.h>

#define LIMIT   200000L

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTFSKM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

static int fill(void)
{
    FILE    *fp = fopen("DD:WORK", "w");
    int     i;

    if (!fp) return -1;
    for (i = 0; i < 5; i++) fputs("ABCDEFGH\n", fp);
    return fclose(fp);
}

/* (a) and (b) */
static int readcase(void)
{
    FILE    *fp;
    int     rc;
    int     c;

    if (fill()) {
        wtof("TSTFSKM cannot write DD WORK - CANNOT MEASURE");
        return 12;
    }
    fp = fopen("DD:WORK", "r");
    if (!fp) {
        wtof("TSTFSKM cannot read DD WORK - CANNOT MEASURE");
        return 12;
    }

    rc = fseek(fp, 100000L, SEEK_SET);
    wtof("TSTFSKM (a) seek past end: rc=%d ferror=%d feof=%d",
         rc, ferror(fp), feof(fp));
    if (rc == 0 || !ferror(fp)) {
        wtof("TSTFSKM no error indicator to test - CANNOT MEASURE");
        fclose(fp);
        return 12;
    }

    rc = fseek(fp, 0L, SEEK_SET);
    wtof("TSTFSKM (a) fseek 0: rc=%d ferror=%d feof=%d",
         rc, ferror(fp), feof(fp));
    want(rc == 0, "(a) fseek() to 0 succeeds on a stream in error");
    want(ferror(fp) != 0, "(a) fseek() keeps the error indicator");
    want(feof(fp) == 0, "(a) fseek() clears the end-of-file indicator");
    c = fgetc(fp);
    want(c == EOF, "(a) a stream still in error does not read");

    rewind(fp);
    wtof("TSTFSKM (a) rewind: ferror=%d feof=%d", ferror(fp), feof(fp));
    want(ferror(fp) == 0 && feof(fp) == 0, "(a) rewind() clears both");
    c = fgetc(fp);
    want(c == 'A', "(a) after rewind() the stream reads from the start");

    /* (b) */
    while (fgetc(fp) != EOF) ;
    wtof("TSTFSKM (b) read to end: ferror=%d feof=%d", ferror(fp), feof(fp));
    want(feof(fp) != 0 && ferror(fp) == 0, "(b) control: end-of-file alone");
    rc = fseek(fp, 0L, SEEK_SET);
    want(rc == 0 && feof(fp) == 0 && ferror(fp) == 0,
         "(b) fseek() clears end-of-file");
    want(fgetc(fp) == 'A', "(b) and reads from the start");

    fclose(fp);
    return 0;
}

/* (c) */
static int writecase(void)
{
    FILE    *fp = fopen("DD:FULL", "w");
    long    n = 0;
    int     rc;

    if (!fp) {
        wtof("TSTFSKM cannot open DD FULL - CANNOT MEASURE");
        return 12;
    }
    while (n < LIMIT && fputs("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n",
                              fp) != EOF)
        n++;
    wtof("TSTFSKM (c) %ld lines written, ferror=%d", n, ferror(fp));
    if (!ferror(fp)) {
        wtof("TSTFSKM (c) the data set did not fill - CANNOT MEASURE");
        fclose(fp);
        return 12;
    }

    rc = fseek(fp, 0L, SEEK_CUR);
    wtof("TSTFSKM (c) fseek cur: rc=%d ferror=%d", rc, ferror(fp));
    want(rc == 0, "(c) fseek() to where it is succeeds");
    want(ferror(fp) != 0, "(c) fseek() keeps the error indicator");
    errno = 0;
    rc = fputs("Y\n", fp);
    want(rc == EOF, "(c) the next write still fails");

    clearerr(fp);
    fclose(fp);
    return 0;
}

int main(void)
{
    int     rc;

    rc = readcase();
    if (rc) return rc;
    rc = writecase();
    if (rc) return rc;

    wtof("TSTFSKM VERDICT failed=%d", bad);
    return bad ? 8 : 0;
}
