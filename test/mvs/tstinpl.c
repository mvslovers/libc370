/*
 * tstinpl.c - libc370 #189 slice 2: r+ and w+ overwrite in place.
 *
 * A '+' stream reading a sequential data set opens it UPDAT; a write in
 * the middle replaces bytes of the record at the position, and the record
 * goes back, same length, through @@awrite's update path when the next
 * record is read or the stream closes.  Rules (#189): a record never grows
 * and never moves; on F a '\n' blank-fills the rest of the record; on V a
 * '\n' inside the record would shorten it and is refused (EOPNOTSUPP), as
 * is any byte where the record's own '\n' is.
 *
 * One data set per case, FB 80 unless said; the JCL pre-loads them.
 *   LINEOUT  INA  L1..L5: fseek(162), "Done 3\n", ftell, read on
 *                                      -> L1,L2,Done 3,L4,L5; ftell 243; L4
 *   CHAROUT  INB  L1..L3: fseek(2), "XY"   -> L1XY,L2,L3
 *   PASTEND  INC  L1..L3: fseek(78), "AB" then 'C' on the separator
 *                                      -> 'C' refused errno 45; AB in 79-80
 *   MULTI    IN3  L1..L9, BLKSIZE 240: Done 4 at 243, read, Done 6 at 405,
 *                 read                 -> reads L5 and L7; L1..L3,Done 4,
 *                                         L5,Done 6,L7..L9
 *   WPLUS    INW  empty: w+ "L1\nL2\nL3\n", fseek(81), "X\n" -> L1,X,L3
 *   VBSAME   INV  VB 84, written here with "w" as L1 L2 L3 - 2-byte
 *                 records (IEBGENER from cards would keep 80 bytes each):
 *                 fseek(3), "M2\n"  -> L1,M2,L3
 *   VBSHORT  INV2 the same, fseek(3), "Q\n" -> refused errno 45 at the
 *                 '\n'; the Q stays written -> L1,Q2,L3
 *   MEMBER   INM(MEMR) L1,L2: r+ reads L1, a write -> errno 45, unchanged
 *
 * RC: 0 = every case matched, 8 = at least one did not.  Every verdict also
 * goes to the console via wtof().
 *
 * BUILD (host):
 *
 *     make build
 *     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstinpl.c \
 *           -o TSTINPL -flinker-output=iebcopy
 *     ld370 --pack TSTINPL=TSTINPL.iebcopy -o probe -xmit \
 *           --dsn IBMUSER.LIBC370.T189SCR
 *
 * upload probe.xmit to IBMUSER.LIBC370.T189XMIT, run jcl/recvappn.jcl,
 * then jcl/tstinpl.jcl.
 *
 * MEASURED on mvsdev 2026-09-27, JOB00571, CC 0000 - all eight OK:
 *
 *   LINEOUT  fputs=7 ftell=243 next=[L4] disk=[L1,L2,Done 3,L4,L5]
 *   CHAROUT  disk=[L1XY,L2,L3]
 *   PASTEND  fputc(C)=-1 errno=45 rec1[78..79]=AB
 *   MULTI    after4=[L5] after6=[L7] disk=[L1,L2,L3,Done 4,L5,Done 6,L7,L8,L9]
 *   WPLUS    disk=[L1,X,L3]
 *   VBSAME   fputs=3 disk=[L1,M2,L3]
 *   VBSHORT  fputs=-1 errno=45 disk=[L1,Q2,L3]
 *   MEMBER   read=[L1] fputc=-1 errno=45 disk=[L1,L2]
 *
 * The first run (JOB00565) failed VBSAME/VBSHORT on the TEST DATA: IEBGENER
 * from cards writes VB records of 80 data bytes, so fseek(3) landed inside
 * record 1 and the library, correctly, wrote there and refused the '\n'.
 * The VB data sets are now written by the probe itself.  Before slice 2
 * every write in the middle of a '+' stream answered EOPNOTSUPP (tstplus
 * WMID, JOB00553).
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
    printf("%-8s %s  %s\n", label, ok ? "OK  " : "FAIL", detail);
    wtof("TSTINPL %s %s %s", label, ok ? "OK" : "FAIL", detail);
    if (!ok) bad = 1;
}

static FILE *
rplus(const char *label, const char *fn, const char *mode)
{
    FILE    *fp = fopen(fn, mode);
    char    d[80];

    if (!fp) {
        sprintf(d, "fopen %s NULL errno=%d", mode, errno);
        verdict(label, 0, d);
    }
    return fp;
}

int
main(int argc, char **argv)
{
    FILE    *fp;
    char    a[100];
    char    seen[160];
    char    d[240];
    long    t;
    int     rc, rc2, e, ok;

    /* LINEOUT */
    if ((fp = rplus("LINEOUT", "'IBMUSER.LIBC370.T189.INA'", "r+")) != NULL) {
        rc = fseek(fp, 162, SEEK_SET);
        rc2 = fputs("Done 3\n", fp);
        t = ftell(fp);
        ok = fgets(a, sizeof(a), fp) != NULL;
        if (ok) trim(a);
        fclose(fp);
        contents("'IBMUSER.LIBC370.T189.INA'", seen, sizeof(seen));
        sprintf(d, "seek=%d fputs=%d ftell=%ld next=[%s] disk=[%s]",
                rc, rc2, t, ok ? a : "EOF", seen);
        verdict("LINEOUT", rc == 0 && rc2 >= 0 && t == 243 && ok && !strcmp(a, "L4")
                && !strcmp(seen, "L1,L2,Done 3,L4,L5"), d);
    }

    /* CHAROUT */
    if ((fp = rplus("CHAROUT", "'IBMUSER.LIBC370.T189.INB'", "r+")) != NULL) {
        rc = fseek(fp, 2, SEEK_SET);
        rc2 = fputs("XY", fp);
        fclose(fp);
        contents("'IBMUSER.LIBC370.T189.INB'", seen, sizeof(seen));
        sprintf(d, "seek=%d fputs=%d disk=[%s]", rc, rc2, seen);
        verdict("CHAROUT", rc == 0 && rc2 >= 0 && !strcmp(seen, "L1XY,L2,L3"), d);
    }

    /* PASTEND */
    if ((fp = rplus("PASTEND", "'IBMUSER.LIBC370.T189.INC'", "r+")) != NULL) {
        rc = fseek(fp, 78, SEEK_SET);
        rc2 = fputs("AB", fp);
        errno = 0;
        t = fputc('C', fp);
        e = errno;
        fclose(fp);
        fp = fopen("'IBMUSER.LIBC370.T189.INC'", "r");
        a[0] = 0;
        if (fp) {
            if (fgets(a, sizeof(a), fp)) a[80] = 0;
            fclose(fp);
        }
        ok = strlen(a) == 80 && !memcmp(a, "L1", 2) && !memcmp(a + 78, "AB", 2);
        contents("'IBMUSER.LIBC370.T189.INC'", seen, sizeof(seen));
        sprintf(d, "seek=%d fputs=%d fputc(C)=%ld errno=%d rec1[78..79]=%.2s n=%d",
                rc, rc2, t, e, strlen(a) >= 80 ? a + 78 : "??", (int)strlen(a));
        verdict("PASTEND", rc == 0 && rc2 >= 0 && t == EOF && e == EOPNOTSUPP && ok
                && strstr(seen, ",L2,L3") != NULL, d);
    }

    /* MULTI */
    if ((fp = rplus("MULTI", "'IBMUSER.LIBC370.T189.IN3'", "r+")) != NULL) {
        char b[100];
        fseek(fp, 243, SEEK_SET);
        fputs("Done 4\n", fp);
        ok = fgets(a, sizeof(a), fp) != NULL;
        if (ok) trim(a);
        fseek(fp, 405, SEEK_SET);
        fputs("Done 6\n", fp);
        rc = fgets(b, sizeof(b), fp) != NULL;
        if (rc) trim(b);
        fclose(fp);
        contents("'IBMUSER.LIBC370.T189.IN3'", seen, sizeof(seen));
        sprintf(d, "after4=[%s] after6=[%s] disk=[%s]", ok ? a : "EOF", rc ? b : "EOF", seen);
        verdict("MULTI", ok && !strcmp(a, "L5") && rc && !strcmp(b, "L7")
                && !strcmp(seen, "L1,L2,L3,Done 4,L5,Done 6,L7,L8,L9"), d);
    }

    /* WPLUS */
    if ((fp = rplus("WPLUS", "'IBMUSER.LIBC370.T189.INW'", "w+")) != NULL) {
        fputs("L1\nL2\nL3\n", fp);
        rc = fseek(fp, 81, SEEK_SET);
        rc2 = fputs("X\n", fp);
        fclose(fp);
        contents("'IBMUSER.LIBC370.T189.INW'", seen, sizeof(seen));
        sprintf(d, "seek=%d fputs=%d disk=[%s]", rc, rc2, seen);
        verdict("WPLUS", rc == 0 && rc2 >= 0 && !strcmp(seen, "L1,X,L3"), d);
    }

    /* the VB data sets get short records from a plain "w" */
    if ((fp = fopen("'IBMUSER.LIBC370.T189.INV'", "w")) != NULL) {
        fputs("L1\nL2\nL3\n", fp);
        fclose(fp);
    }
    if ((fp = fopen("'IBMUSER.LIBC370.T189.INV2'", "w")) != NULL) {
        fputs("L1\nL2\nL3\n", fp);
        fclose(fp);
    }

    /* VBSAME */
    if ((fp = rplus("VBSAME", "'IBMUSER.LIBC370.T189.INV'", "r+")) != NULL) {
        rc = fseek(fp, 3, SEEK_SET);
        rc2 = fputs("M2\n", fp);
        fclose(fp);
        contents("'IBMUSER.LIBC370.T189.INV'", seen, sizeof(seen));
        sprintf(d, "seek=%d fputs=%d disk=[%s]", rc, rc2, seen);
        verdict("VBSAME", rc == 0 && rc2 >= 0 && !strcmp(seen, "L1,M2,L3"), d);
    }

    /* VBSHORT */
    if ((fp = rplus("VBSHORT", "'IBMUSER.LIBC370.T189.INV2'", "r+")) != NULL) {
        rc = fseek(fp, 3, SEEK_SET);
        errno = 0;
        rc2 = fputs("Q\n", fp);
        e = errno;
        fclose(fp);
        contents("'IBMUSER.LIBC370.T189.INV2'", seen, sizeof(seen));
        sprintf(d, "seek=%d fputs=%d errno=%d disk=[%s]", rc, rc2, e, seen);
        verdict("VBSHORT", rc == 0 && rc2 == EOF && e == EOPNOTSUPP
                && !strcmp(seen, "L1,Q2,L3"), d);
    }

    /* MEMBER */
    if ((fp = rplus("MEMBER", "'IBMUSER.LIBC370.T189.INM(MEMR)'", "r+")) != NULL) {
        ok = fgets(a, sizeof(a), fp) != NULL;
        if (ok) trim(a);
        errno = 0;
        rc = fputc('X', fp);
        e = errno;
        fclose(fp);
        contents("'IBMUSER.LIBC370.T189.INM(MEMR)'", seen, sizeof(seen));
        sprintf(d, "read=[%s] fputc=%d errno=%d disk=[%s]", ok ? a : "EOF", rc, e, seen);
        verdict("MEMBER", ok && !strcmp(a, "L1") && rc == EOF && e == EOPNOTSUPP
                && !strcmp(seen, "L1,L2"), d);
    }

    printf("TSTINPL RC=%d\n", bad ? 8 : 0);
    return bad ? 8 : 0;
}
