/*
 * tstwrpos.c - libc370 #189: three write-path defects, measured.
 *
 * THIS PROBE DECIDES NOTHING.  It measures three things the code reading
 * for #189 turned up and brexx370 then hit on mvsdev (JOB00493):
 *
 *   EMPTY   fputs("a\n\nb\n") on a text stream.  __fputc() flushes on
 *           '\n', and __fflush() returns early on an empty buffer, so the
 *           empty line should never become a record.  Expected if the
 *           reading is right: 2 records "a","b" instead of 3.
 *
 *   FTELL   ftell() after writes.  __fflush() sets filepos = 0 after every
 *           record, so ftell() on a write stream should count only the
 *           bytes since the last '\n'.  C wants 3, 6, 8 below.
 *
 *   SEEKW   fputs("ABCDEF"), fflush(), fseek(fp, 3, SEEK_SET), fputs("xy")
 *           on a "w" stream.  After the flush filepos is 0, so the seek
 *           runs forward through __fgetc(), which on a write stream hands
 *           back the stale write buffer.  Expected if the reading is right:
 *           records "ABCDEF","ABCxy".
 *
 * EMPTY runs on FB 80 and on VB 84; FTELL and SEEKW on FB 80.  The JCL
 * pre-allocates the targets.  Records are read back with "r" and printed
 * with trailing blanks trimmed.
 *
 * RC: 0 = every case was measured, 8 = an open failed.  Every verdict also
 * goes to the console via wtof().
 *
 * BUILD (host):
 *
 *     make build
 *     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstwrpos.c \
 *           -o TSTWRPOS -flinker-output=iebcopy
 *     ld370 --pack TSTWRPOS=TSTWRPOS.iebcopy -o probe -xmit \
 *           --dsn IBMUSER.LIBC370.T189SCR
 *
 * upload probe.xmit to IBMUSER.LIBC370.T189XMIT, run jcl/recvappn.jcl,
 * then jcl/tstwrpos.jcl.
 *
 * MEASURED on mvsdev 2026-09-27, libc370 main at abb2cec, JOB00495,
 * CC 0000.  All three readings confirmed:
 *
 *   EMPTYF  LOST        n=2 [a|b]          FB 80
 *   EMPTYV  LOST        n=2 [a|b]          VB 84
 *   FTELL   WRONG       0 0 2              C: 3 6 8
 *   SEEKW   DUPLICATED  n=2 [ABCDEF|ABCxy] fseek() answered 0
 *
 * AFTER the #199 fix, JOB00497, CC 0000:
 *
 *   EMPTYF  KEPT        n=3 [a||b]
 *   EMPTYV  KEPT        n=3 [a||b]         RDW-only record; mvsMF reads
 *                                          the same three records back
 *   FTELL / SEEKW unchanged - that is #200.
 *
 * AFTER the #200 fix, JOB00522, CC 0000:
 *
 *   FTELL   OK          3 6 8
 *   SEEKW   REFUSED     rc=-1 errno=29 (ESPIPE) n=2 [ABCDEF|xy]
 *           A write-only stream cannot move until #189 brings w+/r+/a+;
 *           the refused seek leaves the data set as written.
 *   EMPTYF / EMPTYV still KEPT n=3 [a||b].
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

/* read fn back, print every record, return the count; seen = "r1|r2|.." */
static int
readback(const char *label, const char *fn, char *seen, size_t seenlen)
{
    FILE    *fp;
    char    line[100];
    int     n = 0;

    seen[0] = 0;
    fp = fopen(fn, "r");
    if (!fp) {
        printf("%-6s fopen(\"%s\",\"r\") failed errno=%d\n", label, fn, errno);
        wtof("TSTWRPOS %s OPEN-R FAILED errno=%d", label, errno);
        bad = 1;
        return -1;
    }
    while (fgets(line, sizeof(line), fp)) {
        trim(line);
        printf("%-6s   record %d: '%s'\n", label, n + 1, line);
        if (strlen(seen) + strlen(line) + 2 < seenlen) {
            if (n) strcat(seen, "|");
            strcat(seen, line);
        }
        n++;
    }
    fclose(fp);
    return n;
}

static FILE *
openw(const char *label, const char *fn)
{
    FILE *fp = fopen(fn, "w");

    if (!fp) {
        printf("%-6s fopen(\"%s\",\"w\") failed errno=%d\n", label, fn, errno);
        wtof("TSTWRPOS %s OPEN-W FAILED errno=%d", label, errno);
        bad = 1;
    }
    return fp;
}

static void
empty(const char *label, const char *fn)
{
    FILE    *fp = openw(label, fn);
    char    seen[80];
    int     n;

    if (!fp) return;
    fputs("a\n\nb\n", fp);
    fclose(fp);

    n = readback(label, fn, seen, sizeof(seen));
    if (n < 0) return;
    printf("%-6s VERDICT %s (%d records: %s)\n", label,
           n == 3 ? "KEPT" : "LOST", n, seen);
    wtof("TSTWRPOS %s %s n=%d [%s]", label,
         n == 3 ? "KEPT" : "LOST", n, seen);
}

static void
ftells(const char *fn)
{
    FILE    *fp = openw("FTELL", fn);
    long    t1, t2, t3;

    if (!fp) return;
    fputs("L1\n", fp);  t1 = ftell(fp);
    fputs("L2\n", fp);  t2 = ftell(fp);
    fputs("AB", fp);    t3 = ftell(fp);
    fclose(fp);

    printf("FTELL  after L1\\n=%ld L2\\n=%ld AB=%ld (C: 3 6 8) VERDICT %s\n",
           t1, t2, t3, (t1 == 3 && t2 == 6 && t3 == 8) ? "OK" : "WRONG");
    wtof("TSTWRPOS FTELL %s %ld %ld %ld",
         (t1 == 3 && t2 == 6 && t3 == 8) ? "OK" : "WRONG", t1, t2, t3);
}

static void
seekw(const char *fn)
{
    FILE    *fp = openw("SEEKW", fn);
    char    seen[80];
    int     n;
    int     rc;
    int     e;
    const char *verdict;

    if (!fp) return;
    fputs("ABCDEF", fp);
    fflush(fp);
    errno = 0;
    rc = fseek(fp, 3, SEEK_SET);
    e = errno;
    fputs("xy", fp);
    fclose(fp);

    n = readback("SEEKW", fn, seen, sizeof(seen));
    if (n < 0) return;
    if      (n == 1 && strcmp(seen, "ABCxy") == 0)        verdict = "C";
    else if (n == 2 && strcmp(seen, "ABCDEF|ABCxy") == 0) verdict = "DUPLICATED";
    else if (rc != 0 && e == ESPIPE
             && n == 2 && strcmp(seen, "ABCDEF|xy") == 0) verdict = "REFUSED";
    else                                                  verdict = "OTHER";
    printf("SEEKW  fseek rc=%d errno=%d VERDICT %s (%d records: %s)\n",
           rc, e, verdict, n, seen);
    wtof("TSTWRPOS SEEKW %s rc=%d errno=%d n=%d [%s]", verdict, rc, e, n, seen);
}

int
main(int argc, char **argv)
{
    empty("EMPTYF", "'IBMUSER.LIBC370.T189.FB'");
    empty("EMPTYV", "'IBMUSER.LIBC370.T189.VB'");
    ftells("'IBMUSER.LIBC370.T189.FB'");
    seekw("'IBMUSER.LIBC370.T189.FB'");

    printf("TSTWRPOS RC=%d\n", bad ? 8 : 0);
    return bad ? 8 : 0;
}
