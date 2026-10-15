/*
 * tstdddcb.c - libc370 #554: recfm=, lrecl=, blksize= in the mode of a
 *              "DD:" fopen() take precedence over the DD and the DSCB.
 *
 * The DDs come from jcl/tstdddcb.jcl; each is a new temporary data set
 * that is opened, written, closed, and opened again without options, so
 * the second open shows what the DSCB recorded.
 *
 *   (a) OUTA, no DCB on the DD, recfm=fb,lrecl=80,blksize=800 -> FB 80 800
 *       and the reopen without options reads FB 80 800 back from the DSCB
 *   (b) OUTB, DD DCB=(RECFM=FB,LRECL=80,BLKSIZE=1600), blksize=800 and the
 *       rest named too -> 800 wins over 1600
 *   (c) OUTC, DD DCB=(BLKSIZE=400), recfm=fb,lrecl=80,blksize=800 -> 800
 *   (d) OUTD, no DCB, recfm=vba,lrecl=64,blksize=68 -> VBA 64 68
 *   (e) OUTE, no DCB, recfm=u,blksize=100 -> U, BLKSIZE 100
 *   (f) OUTF, DD DCB=(RECFM=FB,LRECL=80,BLKSIZE=1600), plain "wb" -> the
 *       DD's FB 80 1600, unchanged (regression)
 *   (g) OUTA again, read with recfm=fb,lrecl=80,blksize=800: all records
 *   (h) OUTA, recfm=x -> fopen() fails with EINVAL
 *
 * RC 0 = every check passed, 8 = a check failed.  Built twice: TSTDDDCB
 * against this tree's libc.a (green), TSTDDDCR against the installed one
 * (red: the options are ignored for DD:).
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <mvs/wto.h>

#define RECS    25

static int  bad = 0;

static void check(int cond, const char *what)
{
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", what);
    wtof("TSTDDDCB %s %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

typedef struct { unsigned recfm, lrecl, blksize; } ATTR;

/* open name with mode, report the DCB attributes, write RECS records of
   len bytes if writing; 0 ok, -1 open failed (errno kept) */
static int run(const char *name, const char *mode, size_t len, ATTR *a)
{
    FILE    *fp;
    char    rec[256];
    int     n;
    int     eno;

    memset(a, 0, sizeof(*a));
    errno = 0;
    fp = fopen(name, mode);
    if (!fp) {
        eno = errno;
        printf("      %-8s %-40s open failed errno=%d\n", name, mode, eno);
        wtof("TSTDDDCB %s %s open failed errno=%d", name, mode, eno);
        errno = eno;
        return -1;
    }
    a->recfm   = fp->recfm;
    a->lrecl   = fp->lrecl;
    a->blksize = fp->blksize;
    if (mode[0] == 'w') {
        memset(rec, 'R', sizeof(rec));
        for (n = 0; n < RECS; n++) fwrite(rec, 1, len, fp);
    }
    else {
        for (n = 0; fread(rec, 1, sizeof(rec), fp) > 0; n++) ;
        a->lrecl = (a->lrecl << 16) | (unsigned)n;    /* lrecl | records */
    }
    fclose(fp);
    printf("      %-8s %-40s RECFM=%02X LRECL=%u BLKSIZE=%u\n", name, mode,
           a->recfm, mode[0] == 'w' ? a->lrecl : a->lrecl >> 16, a->blksize);
    wtof("TSTDDDCB %s %s RECFM=%02X LRECL=%u BLKSIZE=%u", name, mode,
         a->recfm, mode[0] == 'w' ? a->lrecl : a->lrecl >> 16, a->blksize);
    return 0;
}

int main(void)
{
    ATTR    a;
    int     rc;

    printf("=== tstdddcb: #554 - DCB attributes in the mode of a DD: open ===\n");

    rc = run("DD:OUTA", "wb,record,recfm=fb,lrecl=80,blksize=800", 80, &a);
    check(rc == 0 && a.recfm == 0x90 && a.lrecl == 80 && a.blksize == 800,
          "(a) no DCB on the DD: FB 80 800 from the mode");
    rc = run("DD:OUTA", "rb,record", 0, &a);
    check(rc == 0 && a.recfm == 0x90 && (a.lrecl >> 16) == 80
          && a.blksize == 800 && (a.lrecl & 0xFFFF) == RECS,
          "(a) reopen without options: FB 80 800 from the DSCB");

    rc = run("DD:OUTB", "wb,record,recfm=fb,lrecl=80,blksize=800", 80, &a);
    check(rc == 0 && a.blksize == 800, "(b) mode 800 over DD BLKSIZE=1600");

    rc = run("DD:OUTC", "wb,record,recfm=fb,lrecl=80,blksize=800", 80, &a);
    check(rc == 0 && a.blksize == 800, "(c) mode 800 over DD BLKSIZE=400");

    rc = run("DD:OUTD", "wb,record,recfm=vba,lrecl=64,blksize=68", 0, &a);
    check(rc == 0 && a.recfm == 0x54 && a.lrecl == 64 && a.blksize == 68,
          "(d) VBA 64 68 from the mode");

    rc = run("DD:OUTE", "wb,recfm=u,blksize=100", 50, &a);
    check(rc == 0 && (a.recfm & 0xC0) == 0xC0 && a.blksize == 100,
          "(e) U, BLKSIZE 100 from the mode");

    rc = run("DD:OUTF", "wb,record", 80, &a);
    check(rc == 0 && a.recfm == 0x90 && a.lrecl == 80 && a.blksize == 1600,
          "(f) no options: the DD's FB 80 1600 unchanged");

    rc = run("DD:OUTA", "rb,record,recfm=fb,lrecl=80,blksize=800", 0, &a);
    check(rc == 0 && (a.lrecl & 0xFFFF) == RECS,
          "(g) input with the options: every record read");

    rc = run("DD:OUTA", "rb,recfm=x", 0, &a);
    check(rc == -1 && errno == EINVAL, "(h) recfm=x: EINVAL");

    printf("=== tstdddcb: %d check(s) failed ===\n", bad);
    wtof("TSTDDDCB VERDICT failed=%d", bad);
    return bad ? 8 : 0;
}
