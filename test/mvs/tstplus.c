/*
 * tstplus.c - libc370 #189 slice 1: r+, w+ and a+ on mvsdev.
 *
 * The host test (test/host/tstplus.c) runs the switching logic against an
 * in-memory data set.  This probe asks what only MVS can answer: does a
 * DCB closed one way really reopen the same DD the other way, is EXTEND
 * after OUTPUT really an append, does a member written with "w+" read back
 * after its STOW, and is the byte view (FB 80: 81 bytes per record) what
 * ftell() and the reader agree on.
 *
 * CASES (FB 80 unless said; the JCL pre-loads and pre-allocates):
 *   WPLUS   w+  on an empty data set: L1 L2, ftell, fseek(0), read back,
 *               write L3 at the end            -> L1,L2,L3
 *   WMID    w+  write in the middle            -> slice 1: EOF, errno 45;
 *               since slice 2 it overwrites in place: L1,X2
 *   RPLUS   r+  on L1..L3: read L1, SEEK_END, ftell, write L4 -> L1..L4
 *   APLUS   a+  on L1,L2: ftell, write L3, fseek(0), read L1, write L4
 *                                              -> L1..L4
 *   AREAD   a+  on L1,L2: read without a seek  -> EOF, data unchanged
 *   WMEM    w+  on a new member: L1 L2, fseek(0), read back, write again
 *                                              -> EOF errno 45, L1,L2
 *   RNEW    r+  on a data set that does not exist -> NULL
 *   SYSOUT  w+  on a SYSOUT DD                  -> NULL, errno 22 (EINVAL)
 *   ANEW    a+  on a new member                 -> created, ftell 0
 *   ANEWDS  a+  on a new data set               -> created, ftell 0
 *   RMEM    r+  on an existing member: reads; a write -> EOF errno 45
 *   DDMEM   w+  through a DD with the member in the JCL: write, read back,
 *               write again -> EOF errno 45, the stream falls back to
 *               reading (__fpswt's recovery path)
 *   PARTW, PARTWP, PARTA   "AB", fseek(ftell()), "CD\n" with "w", "w+" and
 *               "a+" -> ONE record ABCD: a seek to where the stream is must
 *               not end a half-written line
 *
 * Every verdict also goes to the console via wtof().
 *
 * RC: 0 = every case matched, 8 = at least one did not.
 *
 * BUILD (host):
 *
 *     make build
 *     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstplus.c \
 *           -o TSTPLUS -flinker-output=iebcopy
 *     ld370 --pack TSTPLUS=TSTPLUS.iebcopy -o probe -xmit \
 *           --dsn IBMUSER.LIBC370.T189SCR
 *
 * upload probe.xmit to IBMUSER.LIBC370.T189XMIT, run jcl/recvappn.jcl,
 * then jcl/tstplus.jcl.
 *
 * MEASURED on mvsdev 2026-09-27, JOB00545, CC 0000 - all eight OK:
 *
 *   WPLUS   ftell=162 seek=0 read=[L1,L2] eof=1 ftell2=243 disk=[L1,L2,L3]
 *   WMID    fputc=-1 errno=45 disk=[L1,L2]
 *   RPLUS   read=[L1] seek=0 ftell=243 disk=[L1,L2,L3,L4]
 *   APLUS   ftell=162 seek=0 read=[L1] disk=[L1,L2,L3,L4]
 *   AREAD   read=EOF disk=[L1,L2]
 *   WMEM    seek=0 read=[L1,L2] fputs=-1 errno=45 disk=[L1,L2]
 *   RNEW    fopen=NULL
 *   SYSOUT  fopen=NULL errno=22
 *
 * After the review fixes (a seek to the current position does not flush;
 * "a+" counts its size at open), JOB00553, CC 0000 - all fifteen OK, the
 * eight above unchanged, plus:
 *
 *   ANEW    ftell=0 seek=0 read=[L1] disk=[L1]
 *   ANEWDS  ftell=0 seek=0 read=[L1] more=0 disk=[L1]
 *   RMEM    read=[L1] fputs=-1 errno=45 disk=[L1,L2]
 *   DDMEM   seek=0 read=[L1] fputs=-1 errno=45 ftell=162 disk=[L1,L2]
 *           (@@aopen -45, and __fpswt() fell back to reading at 162)
 *   PARTW   ftell=2 seek=0 disk=[ABCD]
 *   PARTWP  ftell=2 seek=0 disk=[ABCD]
 *   PARTA   ftell=164 seek=0 disk=[L1,L2,ABCD]
 *
 * With #189 slice 2 (in place), JOB00572: all fifteen OK; WMID now reads
 * fputc=231 ('X') disk=[L1,X2] - the write in the middle overwrites.
 *
 * Before slice 1 every one of these opens returned NULL: __fpmode()
 * refused '+'.  Regression on the same library: tstwrpos JOB00547 (FTELL
 * now 81 162 164, the byte view) and tstappnd JOB00548, unchanged.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <mvs/wto.h>

static int bad = 0;

static void
trim(char *s)
{
    size_t n = strlen(s);

    while (n > 0 && (s[n-1] == '\n' || s[n-1] == ' ')) s[--n] = 0;
}

/* the data set as a separate "r" open sees it */
static void
contents(const char *fn, char *seen, size_t len)
{
    FILE    *fp = fopen(fn, "r");
    char    line[100];

    seen[0] = 0;
    if (!fp) {
        strcpy(seen, "?OPEN");
        return;
    }
    while (fgets(line, sizeof(line), fp)) {
        trim(line);
        if (strlen(seen) + strlen(line) + 2 < len) {
            if (seen[0]) strcat(seen, ",");
            strcat(seen, line);
        }
    }
    fclose(fp);
}

static void
verdict(const char *label, int ok, const char *detail)
{
    printf("%-7s %s  %s\n", label, ok ? "OK  " : "FAIL", detail);
    wtof("TSTPLUS %s %s %s", label, ok ? "OK" : "FAIL", detail);
    if (!ok) bad = 1;
}

static int
getline1(FILE *fp, char *buf, size_t len)
{
    if (!fgets(buf, (int)len, fp)) return 0;
    trim(buf);
    return 1;
}

int
main(int argc, char **argv)
{
    FILE    *fp;
    char    a[100], b[100], c[100];
    char    seen[120];
    char    d[200];
    long    t1, t2;
    int     rc, e, ok;

    /* WPLUS */
    fp = fopen("'IBMUSER.LIBC370.T189.PLW'", "w+");
    if (!fp) {
        sprintf(d, "fopen w+ NULL errno=%d", errno);
        verdict("WPLUS", 0, d);
    }
    else {
        fputs("L1\nL2\n", fp);
        t1 = ftell(fp);
        rc = fseek(fp, 0, SEEK_SET);
        ok = getline1(fp, a, sizeof(a)) & getline1(fp, b, sizeof(b));
        e  = getline1(fp, c, sizeof(c));    /* expect EOF */
        fputs("L3\n", fp);
        t2 = ftell(fp);
        fclose(fp);
        contents("'IBMUSER.LIBC370.T189.PLW'", seen, sizeof(seen));
        sprintf(d, "ftell=%ld seek=%d read=[%s,%s] eof=%d ftell2=%ld disk=[%s]",
                t1, rc, ok ? a : "?", ok ? b : "?", !e, t2, seen);
        verdict("WPLUS", t1 == 162 && rc == 0 && ok && !strcmp(a, "L1")
                && !strcmp(b, "L2") && !e && t2 == 243
                && !strcmp(seen, "L1,L2,L3"), d);
    }

    /* WMID */
    fp = fopen("'IBMUSER.LIBC370.T189.PLW'", "w+");
    if (fp) {
        fputs("L1\nL2\n", fp);
        fseek(fp, 0, SEEK_SET);
        getline1(fp, a, sizeof(a));
        errno = 0;
        rc = fputc('X', fp);
        e = errno;
        fclose(fp);
        contents("'IBMUSER.LIBC370.T189.PLW'", seen, sizeof(seen));
        sprintf(d, "fputc=%d errno=%d disk=[%s]", rc, e, seen);
        /* slice 2: the write overwrites in place (was EOPNOTSUPP in
           slice 1, JOB00545/00553) */
        verdict("WMID", rc == 'X' && !strcmp(seen, "L1,X2"), d);
    }
    else verdict("WMID", 0, "fopen w+ NULL");

    /* RPLUS */
    fp = fopen("'IBMUSER.LIBC370.T189.PLR'", "r+");
    if (!fp) {
        sprintf(d, "fopen r+ NULL errno=%d", errno);
        verdict("RPLUS", 0, d);
    }
    else {
        ok = getline1(fp, a, sizeof(a));
        rc = fseek(fp, 0, SEEK_END);
        t1 = ftell(fp);
        fputs("L4\n", fp);
        fclose(fp);
        contents("'IBMUSER.LIBC370.T189.PLR'", seen, sizeof(seen));
        sprintf(d, "read=[%s] seek=%d ftell=%ld disk=[%s]", ok ? a : "?", rc, t1, seen);
        verdict("RPLUS", ok && !strcmp(a, "L1") && rc == 0 && t1 == 243
                && !strcmp(seen, "L1,L2,L3,L4"), d);
    }

    /* APLUS */
    fp = fopen("'IBMUSER.LIBC370.T189.PLA'", "a+");
    if (!fp) {
        sprintf(d, "fopen a+ NULL errno=%d", errno);
        verdict("APLUS", 0, d);
    }
    else {
        t1 = ftell(fp);
        fputs("L3\n", fp);
        rc = fseek(fp, 0, SEEK_SET);
        ok = getline1(fp, a, sizeof(a));
        fputs("L4\n", fp);
        fclose(fp);
        contents("'IBMUSER.LIBC370.T189.PLA'", seen, sizeof(seen));
        sprintf(d, "ftell=%ld seek=%d read=[%s] disk=[%s]", t1, rc, ok ? a : "?", seen);
        verdict("APLUS", t1 == 162 && rc == 0 && ok && !strcmp(a, "L1")
                && !strcmp(seen, "L1,L2,L3,L4"), d);
    }

    /* AREAD */
    fp = fopen("'IBMUSER.LIBC370.T189.PLB'", "a+");
    if (fp) {
        ok = getline1(fp, a, sizeof(a));
        fclose(fp);
        contents("'IBMUSER.LIBC370.T189.PLB'", seen, sizeof(seen));
        sprintf(d, "read=%s disk=[%s]", ok ? a : "EOF", seen);
        verdict("AREAD", !ok && !strcmp(seen, "L1,L2"), d);
    }
    else verdict("AREAD", 0, "fopen a+ NULL");

    /* WMEM */
    fp = fopen("'IBMUSER.LIBC370.T189.PLM(MEMW)'", "w+");
    if (!fp) {
        sprintf(d, "fopen w+ member NULL errno=%d", errno);
        verdict("WMEM", 0, d);
    }
    else {
        fputs("L1\nL2\n", fp);
        rc = fseek(fp, 0, SEEK_SET);
        ok = getline1(fp, a, sizeof(a)) & getline1(fp, b, sizeof(b));
        errno = 0;
        t1 = fputs("L3\n", fp);
        e = errno;
        fclose(fp);
        contents("'IBMUSER.LIBC370.T189.PLM(MEMW)'", seen, sizeof(seen));
        sprintf(d, "seek=%d read=[%s,%s] fputs=%ld errno=%d disk=[%s]",
                rc, ok ? a : "?", ok ? b : "?", t1, e, seen);
        verdict("WMEM", rc == 0 && ok && !strcmp(a, "L1") && !strcmp(b, "L2")
                && t1 == EOF && e == EOPNOTSUPP && !strcmp(seen, "L1,L2"), d);
    }

    /* RNEW */
    errno = 0;
    fp = fopen("'IBMUSER.LIBC370.T189.PLN'", "r+");
    e = errno;
    if (fp) fclose(fp);
    sprintf(d, "fopen=%s errno=%d", fp ? "ok" : "NULL", e);
    verdict("RNEW", fp == NULL, d);

    /* SYSOUT */
    errno = 0;
    fp = fopen("DD:SYSAPP", "w+");
    e = errno;
    if (fp) fclose(fp);
    sprintf(d, "fopen=%s errno=%d", fp ? "ok" : "NULL", e);
    verdict("SYSOUT", fp == NULL && e == EINVAL, d);

    /* ANEW: a+ on a member that does not exist yet - created */
    fp = fopen("'IBMUSER.LIBC370.T189.PLM(MEMA)'", "a+");
    if (!fp) {
        sprintf(d, "fopen a+ new member NULL errno=%d", errno);
        verdict("ANEW", 0, d);
    }
    else {
        t1 = ftell(fp);
        fputs("L1\n", fp);
        rc = fseek(fp, 0, SEEK_SET);
        ok = getline1(fp, a, sizeof(a));
        fclose(fp);
        contents("'IBMUSER.LIBC370.T189.PLM(MEMA)'", seen, sizeof(seen));
        sprintf(d, "ftell=%ld seek=%d read=[%s] disk=[%s]", t1, rc, ok ? a : "?", seen);
        verdict("ANEW", t1 == 0 && rc == 0 && ok && !strcmp(a, "L1") && !strcmp(seen, "L1"), d);
    }

    /* ANEWDS: a+ on a data set that does not exist yet - created */
    fp = fopen("'IBMUSER.LIBC370.T189.PLN2'", "a+");
    if (!fp) {
        sprintf(d, "fopen a+ new data set NULL errno=%d", errno);
        verdict("ANEWDS", 0, d);
    }
    else {
        t1 = ftell(fp);
        fputs("L1\n", fp);
        rc = fseek(fp, 0, SEEK_SET);
        ok = getline1(fp, a, sizeof(a));
        e  = getline1(fp, b, sizeof(b));
        fclose(fp);
        contents("'IBMUSER.LIBC370.T189.PLN2'", seen, sizeof(seen));
        sprintf(d, "ftell=%ld seek=%d read=[%s] more=%d disk=[%s]", t1, rc, ok ? a : "?", e, seen);
        verdict("ANEWDS", t1 == 0 && rc == 0 && ok && !strcmp(a, "L1") && !e
                && !strcmp(seen, "L1"), d);
    }

    /* RMEM: r+ on an existing member - reads, cannot write */
    fp = fopen("'IBMUSER.LIBC370.T189.PLM(MEMR)'", "r+");
    if (!fp) {
        sprintf(d, "fopen r+ member NULL errno=%d", errno);
        verdict("RMEM", 0, d);
    }
    else {
        ok = getline1(fp, a, sizeof(a));
        fseek(fp, 0, SEEK_END);
        errno = 0;
        rc = fputs("L3\n", fp);
        e = errno;
        fclose(fp);
        contents("'IBMUSER.LIBC370.T189.PLM(MEMR)'", seen, sizeof(seen));
        sprintf(d, "read=[%s] fputs=%d errno=%d disk=[%s]", ok ? a : "?", rc, e, seen);
        verdict("RMEM", ok && !strcmp(a, "L1") && rc == EOF && e == EOPNOTSUPP
                && !strcmp(seen, "L1,L2"), d);
    }

    /* DDMEM: w+ through a DD whose MEMBER is in the JCL - the C side does
       not see the member, so writing after a read reaches @@aopen's -45
       and __fpswt() has to fall back to reading where it was */
    fp = fopen("DD:DDMEM", "w+");
    if (!fp) {
        sprintf(d, "fopen w+ DD:DDMEM NULL errno=%d", errno);
        verdict("DDMEM", 0, d);
    }
    else {
        fputs("L1\nL2\n", fp);
        rc = fseek(fp, 0, SEEK_SET);
        ok = getline1(fp, a, sizeof(a));
        fseek(fp, 0, SEEK_END);
        errno = 0;
        t1 = fputs("L3\n", fp);
        e = errno;
        t2 = ftell(fp);
        fclose(fp);
        contents("DD:DDMEM", seen, sizeof(seen));
        sprintf(d, "seek=%d read=[%s] fputs=%ld errno=%d ftell=%ld disk=[%s]",
                rc, ok ? a : "?", t1, e, t2, seen);
        verdict("DDMEM", rc == 0 && ok && !strcmp(a, "L1") && t1 == EOF
                && e == EOPNOTSUPP && !strcmp(seen, "L1,L2"), d);
    }

    /* PARTW / PARTWP / PARTA: one line from several writes with a seek
       to the current position between them - one record, not two */
    fp = fopen("'IBMUSER.LIBC370.T189.PLP'", "w");
    if (fp) {
        fputs("AB", fp);
        t1 = ftell(fp);
        rc = fseek(fp, t1, SEEK_SET);
        fputs("CD\n", fp);
        fclose(fp);
        contents("'IBMUSER.LIBC370.T189.PLP'", seen, sizeof(seen));
        sprintf(d, "ftell=%ld seek=%d disk=[%s]", t1, rc, seen);
        verdict("PARTW", t1 == 2 && rc == 0 && !strcmp(seen, "ABCD"), d);
    }
    else verdict("PARTW", 0, "fopen w NULL");

    fp = fopen("'IBMUSER.LIBC370.T189.PLQ'", "w+");
    if (fp) {
        fputs("AB", fp);
        t1 = ftell(fp);
        rc = fseek(fp, t1, SEEK_SET);
        fputs("CD\n", fp);
        fclose(fp);
        contents("'IBMUSER.LIBC370.T189.PLQ'", seen, sizeof(seen));
        sprintf(d, "ftell=%ld seek=%d disk=[%s]", t1, rc, seen);
        verdict("PARTWP", t1 == 2 && rc == 0 && !strcmp(seen, "ABCD"), d);
    }
    else verdict("PARTWP", 0, "fopen w+ NULL");

    fp = fopen("'IBMUSER.LIBC370.T189.PLS'", "a+");
    if (fp) {
        fputs("AB", fp);
        t1 = ftell(fp);
        rc = fseek(fp, t1, SEEK_SET);
        fputs("CD\n", fp);
        fclose(fp);
        contents("'IBMUSER.LIBC370.T189.PLS'", seen, sizeof(seen));
        sprintf(d, "ftell=%ld seek=%d disk=[%s]", t1, rc, seen);
        verdict("PARTA", t1 == 164 && rc == 0 && !strcmp(seen, "L1,L2,ABCD"), d);
    }
    else verdict("PARTA", 0, "fopen a+ NULL");

    printf("TSTPLUS RC=%d\n", bad ? 8 : 0);
    return bad ? 8 : 0;
}
