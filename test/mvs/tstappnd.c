/*
 * tstappnd.c - libc370 #189: does fopen(..., "a") append, or truncate?
 *
 * THIS PROBE DECIDES NOTHING.  It measures what "a" does today, before
 * any of #189's update modes are designed on top of it.
 *
 * WHY THE QUESTION.  __fpmode() sets _FILE_FLAG_APPEND for "a" and nothing
 * reads that flag afterwards.  __fpopen() hands __aopen() mode 0 or 1 only,
 * so "a" is OPEN OUTPUT, not OPEN EXTEND, and fopen() allocates a data set
 * name DISP=OLD (a member DISP=SHR).  Read from the code, "a" behaves like
 * "w" unless the JCL itself says DISP=MOD.  That is a reading, not a
 * measurement - this is the measurement.
 *
 * FOUR CASES.  The JCL pre-loads each target with one record, "L1".  The
 * probe opens it with "a", writes "L2\n", closes, reopens with "r" and
 * reports what is there.
 *
 *   SEQDSN   'IBMUSER.LIBC370.T189.SEQA'           by name  (DISP=OLD)
 *   MEMDSN   'IBMUSER.LIBC370.T189.PDS(MEMA)'      by name  (DISP=SHR)
 *   DDOLD    DD:DDOLD   DISP=OLD in the JCL
 *   DDMOD    DD:DDMOD   DISP=MOD in the JCL        - the control: MOD
 *                                                    positions at the end
 *                                                    whatever OPEN says
 *
 * ADDED for the #198 fix, each run after the four above:
 *
 *   NEWDSN   'IBMUSER.LIBC370.T189.SEQN'   does not exist: "a" creates it
 *   MEMNEW   'IBMUSER.LIBC370.T189.PDS(MEMN)'  no such member: created
 *   SYSOUT   DD:SYSAPP  SYSOUT=*   EXTEND is not for spool; __aopen()
 *                                  makes it OUTPUT.  Opens and writes?
 *   DDMEM    DD:DDMEM   the MEMBER is in the JCL, DSN=...PDS(MEMB) - the
 *                       JFCB path, which __aopen()'s member check does not
 *                       see.  LAST, because what OPEN EXTEND does to a
 *                       member there is the one thing not yet measured.
 *
 * VERDICT per case, from the records read back:
 *   APPEND   = L1, L2          "a" appended
 *   TRUNC    = L2              "a" overwrote - it is "w"
 *   CREATED  = L2              on a target that did not exist
 *   REFUSED  = fopen() NULL, errno EOPNOTSUPP (45), still L1
 *   OPENED   = SYSOUT only: fopen() and fputs() succeeded
 *   OTHER    = anything else, printed in full
 *
 * RC: 0 = every case was measured (whatever the verdict), 8 = a case
 * could not be measured (an open failed).  Every line also goes to the
 * console via wtof(), so the job log carries it even if SYSOUT is lost.
 *
 * BUILD (host):
 *
 *     make build
 *     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstappnd.c \
 *           -o TSTAPPND -flinker-output=iebcopy
 *     ld370 --pack TSTAPPND=TSTAPPND.iebcopy -o probe -xmit \
 *           --dsn IBMUSER.LIBC370.T189SCR
 *
 * upload probe.xmit to IBMUSER.LIBC370.T189XMIT, run jcl/recvappn.jcl,
 * then jcl/tstappnd.jcl.
 *
 * MEASURED on mvsdev 2026-09-27, libc370 main at abb2cec, JOB00490,
 * CC 0000:
 *
 *   SEQDSN  TRUNC   n=1 [L2]
 *   MEMDSN  TRUNC   n=1 [L2]
 *   DDOLD   TRUNC   n=1 [L2]
 *   DDMOD   APPEND  n=2 [L1,L2]      control
 *
 * "a" is "w".  It appends only where the JCL already said DISP=MOD, i.e.
 * where the system appends whatever the program asks for.  fputs() and
 * ferror() report success in every case, so nothing tells the caller the
 * old records are gone.
 *
 * #198 RED baseline with the new cases, JOB00536, CC 0000: SEQDSN, MEMDSN,
 * DDOLD and DDMEM all TRUNC; DDMOD APPEND; NEWDSN and MEMNEW CREATED;
 * SYSOUT OPENED.
 *
 * With EXTEND alone, JOB00538: DDMEM - the member named in the JCL - went
 * through OPEN EXTEND and ABENDed at CLOSE, SB14-04 (IEC217I, IGG0201Z):
 * STOW cannot add a member that is already in the directory.  The member
 * kept "L1".  That is why @@aopen.asm now refuses EXTEND for a JCL member
 * too (rc -45).
 *
 * AFTER the #198 fix, JOB00540, CC 0000:
 *
 *   SEQDSN  APPEND   n=2 [L1,L2]
 *   MEMDSN  REFUSED  n=1 [L1]   errno=45 (EOPNOTSUPP)
 *   DDOLD   APPEND   n=2 [L1,L2]
 *   DDMOD   APPEND   n=2 [L1,L2]         control
 *   NEWDSN  CREATED  n=1 [L2]
 *   MEMNEW  CREATED  n=1 [L2]
 *   SYSOUT  OPENED   fputs=38
 *   DDMEM   REFUSED  n=1 [L1]   errno=45
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

/* read fn back; returns the record count, -1 if it cannot be opened */
static int
readback(const char *label, const char *fn, char *seen, size_t seenlen)
{
    FILE    *fp;
    char    line[100];
    int     n = 0;

    seen[0] = 0;
    fp = fopen(fn, "r");
    if (!fp) return -1;
    while (fgets(line, sizeof(line), fp)) {
        trim(line);
        printf("%-7s   record %d: '%s'\n", label, n + 1, line);
        if (strlen(seen) + strlen(line) + 2 < seenlen) {
            if (n) strcat(seen, ",");
            strcat(seen, line);
        }
        n++;
    }
    fclose(fp);
    return n;
}

/* existed: the JCL pre-loaded the target with "L1" */
static void
probe(const char *label, const char *fn, int existed)
{
    FILE    *fp;
    char    seen[80];
    int     n;
    int     rc = 0;
    int     e;
    const char *verdict;

    errno = 0;
    fp = fopen(fn, "a");
    e = errno;
    if (fp) {
        rc = fputs("L2\n", fp);
        printf("%-7s \"a\" open: dsn=%s fputs=%d ferror=%d\n",
               label, fp->dataset, rc, ferror(fp));
        fclose(fp);
    }
    else {
        printf("%-7s fopen(\"%s\",\"a\") -> NULL errno=%d\n", label, fn, e);
    }

    n = readback(label, fn, seen, sizeof(seen));
    if (n < 0) {
        printf("%-7s fopen(\"%s\",\"r\") failed errno=%d\n", label, fn, errno);
        wtof("TSTAPPND %s OPEN-R FAILED a=%s errno=%d", label, fp ? "ok" : "NULL", e);
        bad = 1;
        return;
    }

    if (!fp && e == EOPNOTSUPP && n == 1 && strcmp(seen, "L1") == 0)
                                                    verdict = "REFUSED";
    else if (!fp)                                   verdict = "OTHER";
    else if (n == 2 && strcmp(seen, "L1,L2") == 0)  verdict = "APPEND";
    else if (n == 1 && strcmp(seen, "L2") == 0)     verdict = existed ? "TRUNC"
                                                                      : "CREATED";
    else                                            verdict = "OTHER";

    printf("%-7s VERDICT %s (%d records: %s)\n", label, verdict, n, seen);
    wtof("TSTAPPND %s %s n=%d [%s] errno=%d", label, verdict, n, seen, e);
}

/* SYSOUT cannot be read back: did "a" open and write? */
static void
sysout(const char *label, const char *fn)
{
    FILE    *fp;
    int     rc = -2;
    int     e;

    errno = 0;
    fp = fopen(fn, "a");
    e = errno;
    if (fp) {
        rc = fputs("TSTAPPND SYSOUT LINE WRITTEN WITH \"a\"\n", fp);
        if (ferror(fp)) rc = -3;
        fclose(fp);
    }
    printf("%-7s VERDICT %s fopen=%s errno=%d fputs=%d\n", label,
           (fp && rc >= 0) ? "OPENED" : "OTHER", fp ? "ok" : "NULL", e, rc);
    wtof("TSTAPPND %s %s fopen=%s errno=%d fputs=%d", label,
         (fp && rc >= 0) ? "OPENED" : "OTHER", fp ? "ok" : "NULL", e, rc);
}

int
main(int argc, char **argv)
{
    probe("SEQDSN", "'IBMUSER.LIBC370.T189.SEQA'", 1);
    probe("MEMDSN", "'IBMUSER.LIBC370.T189.PDS(MEMA)'", 1);
    probe("DDOLD",  "DD:DDOLD", 1);
    probe("DDMOD",  "DD:DDMOD", 1);
    probe("NEWDSN", "'IBMUSER.LIBC370.T189.SEQN'", 0);
    probe("MEMNEW", "'IBMUSER.LIBC370.T189.PDS(MEMN)'", 0);
    sysout("SYSOUT", "DD:SYSAPP");
    probe("DDMEM",  "DD:DDMEM", 1);     /* last: unmeasured OPEN path */

    printf("TSTAPPND RC=%d\n", bad ? 8 : 0);
    return bad ? 8 : 0;
}
