/*
 * tststdm.c - libc370 #474 on MVS: fwrite() of nothing, fseek() after
 * ungetc() on a + stream and past the end of a record stream, and the
 * mode options of a temporary and a SYSOUT data set.
 *
 *   1 fwrite(p, 0, 5, f) and fwrite(p, 5, 0, f) on a byte stream return 0
 *   2 w+ stream "ABCDEF": fgetc() 'A', ungetc('X'), fseek(f, 1, SEEK_SET):
 *     the next fgetc() is 'B', the data, not 'X'
 *   3 "r,record" on DD RECS (3 records): fseek(f, 2, SEEK_SET) works,
 *     then fseek(f, 10, SEEK_SET) fails and sets the end-of-file indicator
 *   4 &&T: "w,recfm=fb,lrecl=80,blksize=800" opens FB with BLKSIZE 800
 *   5 &&T: "...,space=trk(1,1)" and "...,space=cyl(1,1)" open, and take
 *     200 records.  Not checked here, read from the job log: green
 *     allocates them on VIO (IEF237I VIO), red fell back to a DASD
 *     allocation after the VIO one failed
 *   6 SYSOUT "*": "w,recfm=fb,lrecl=80,blksize=800" opens with BLKSIZE 800
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tststdm.c \
 *         -o $S/TSTSTDG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tststdm.c \
 *         -o $S/TSTSTDR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTSTDG=TSTSTDG.iebcopy \
 *         TSTSTDR=TSTSTDR.iebcopy -o sd -xmit --dsn IBMUSER.LIBC370.SDSCR
 *
 * then jcl/recvstdm.jcl and jcl/tststdm.jcl (DD RECS per step).
 *
 * RC 0 = passed, 8 = failed.
 */
#include <stdio.h>
#include <string.h>
#include <mvs/wto.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTSTDM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

static void opened(const char *name, const char *mode, int blk, int n)
{
    FILE    *f = fopen(name, mode);
    char    rec[80];
    int     i, w = 0;

    if (!f) {
        wtof("TSTSTDM   %s \"%s\": fopen NULL", name, mode);
        want(0, mode);
        return;
    }
    memset(rec, 'R', sizeof(rec));
    for (i = 0; i < n; i++) w += (int)fwrite(rec, sizeof(rec), 1, f);
    wtof("TSTSTDM   %s \"%s\": DD %s recfm %02X lrecl %u blksize %u, %d written",
         name, mode, f->ddname, f->recfm, f->lrecl, f->blksize, w);
    want((!blk || f->blksize == blk) && w == n, mode);
    fclose(f);
}

int main(void)
{
    FILE    *f;
    size_t  a, b;
    int     c, r1, r2, eof;

    f = fopen("&&TW", "w");
    a = fwrite("abcde", 0, 5, f);
    b = fwrite("abcde", 5, 0, f);
    wtof("TSTSTDM   fwrite(p,0,5) %u, fwrite(p,5,0) %u", (unsigned)a, (unsigned)b);
    want(a == 0 && b == 0, "fwrite() of nothing returns 0");
    fclose(f);

    f = fopen("&&TP", "w+");
    fputs("ABCDEF\n", f);
    fseek(f, 0, SEEK_SET);
    c = fgetc(f);
    ungetc('X', f);
    /* to where it is: the early return that kept the character */
    r1 = fseek(f, 1, SEEK_SET);
    c = fgetc(f);
    wtof("TSTSTDM   w+: fseek %d, then fgetc '%c'", r1, c);
    want(r1 == 0 && c == 'B', "fseek() on a + stream drops the ungetc() character");
    fclose(f);

    f = fopen("dd:RECS", "r,record");
    if (f) {
        /* forward only: a seek back reopens, which DD * does not allow */
        r2 = fseek(f, 2, SEEK_SET);
        r1 = fseek(f, 10, SEEK_SET);
        eof = feof(f) != 0;
        wtof("TSTSTDM   record: fseek(2) %d, fseek(10) %d eof %d", r2, r1, eof);
        want(r2 == 0, "fseek() within a record stream works");
        want(r1 != 0 && eof, "fseek() past the end of a record stream fails");
        fclose(f);
    }
    else want(0, "dd:RECS opens");

    opened("&&TB", "w,recfm=fb,lrecl=80,blksize=800", 800, 10);
    opened("&&TT", "w,recfm=fb,lrecl=80,blksize=800,space=trk(1,1)", 800, 200);
    opened("&&TC", "w,recfm=fb,lrecl=80,blksize=800,space=cyl(1,1)", 800, 200);
    opened("*", "w,recfm=fb,lrecl=80,blksize=800", 800, 2);
    return bad ? 8 : 0;
}
