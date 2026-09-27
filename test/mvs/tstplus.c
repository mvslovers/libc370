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
 *   WMID    w+  write in the middle            -> EOF, errno 45, L1,L2
 *   RPLUS   r+  on L1..L3: read L1, SEEK_END, ftell, write L4 -> L1..L4
 *   APLUS   a+  on L1,L2: ftell, write L3, fseek(0), read L1, write L4
 *                                              -> L1..L4
 *   AREAD   a+  on L1,L2: read without a seek  -> EOF, data unchanged
 *   WMEM    w+  on a new member: L1 L2, fseek(0), read back, write again
 *                                              -> EOF errno 45, L1,L2
 *   RNEW    r+  on a data set that does not exist -> NULL
 *   SYSOUT  w+  on a SYSOUT DD                  -> NULL, errno 22 (EINVAL)
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
 * Before slice 1 every one of these opens returned NULL: __fpmode()
 * refused '+'.  Regression on the same library: tstwrpos JOB00547 (FTELL
 * now 81 162 164, the byte view) and tstappnd JOB00548, unchanged.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <clibwto.h>

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
        verdict("WMID", rc == EOF && e == EOPNOTSUPP && !strcmp(seen, "L1,L2"), d);
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

    printf("TSTPLUS RC=%d\n", bad ? 8 : 0);
    return bad ? 8 : 0;
}
