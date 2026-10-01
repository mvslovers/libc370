/*
 * tstidcsp.c - libc370 #71: idcams_sysprint() on a real IDCAMS (MVS, batch).
 *
 * The host test (test/host/tstidcam.c) drives the output exit directly; it
 * cannot follow the exit's data through IDCAMS, whose parameter list holds
 * 31-bit words.  This runs IDCAMS and prints every line the callback gets,
 * which also settles what <mvs/idcams.h> documents from measurement:
 *
 *   - how msgno is encoded (binary 3012 for IDC3012I, or otherwise)
 *   - which message numbers IDCAMS's summary lines carry
 *   - what the record's first byte is (carriage control or text)
 *
 * Every command names a data set that does not exist, under the test's own
 * qualifier, so nothing on the system changes:
 *
 *   1. DELETE of a missing entry          -> rc 8, IDC3012I expected
 *   2. ALTER ... NEWNAME of a missing one -> rc 8, IDC3012I expected
 *   3. ALTER ... NEWNAME, 9-char qualifier -> rc 12, IDC3203I expected
 *   4. LISTCAT of an empty level           -> rc 4 or 0, lines either way
 *   5. idcams() on case 1                  -> the same rc, no callback
 *
 * Pass: cases 1 and 3 report their IDC number through the callback, every
 * case delivers at least one line, and idcams_sysprint() returns what
 * idcams() returns.  The listing is the measurement; keep the job output.
 *
 * Measured on mvsdev, 2026-10-01: JOB01058 (CC 8) showed IDCAMS's own
 * ioflags->msgno is not the IDC number (IDC3012I arrives as 12, IDC0002I as
 * -2) and the record's first byte is carriage control; with the number read
 * from the record, JOB01060 passed (CC 0).
 *
 * PARM='<hlq>'   (default IBMUSER) -- the data set names are <hlq>.TSTIDCSP.*
 *
 * BUILD (host):
 *     python3 sdk/mklibc.py build
 *     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstidcsp.c -o TSTIDCSP
 */
#include <stdio.h>
#include <string.h>
#include <mvs/idcams.h>

static int fails;
#define CHECK(cond, msg) \
    do { if (cond) printf("  ok   %s\n", msg); \
         else { fails++; printf("  FAIL %s\n", msg); } } while (0)

typedef struct {
    int     lines;
    int     want;           /* message number the case expects, 0 = none */
    int     found;          /* seen it */
} SEEN;

static void show(void *arg, int msgno, const char *text, int len)
{
    SEEN    *s  = arg;
    int     n   = len > 100 ? 100 : len;
    int     i;

    s->lines++;
    if (s->want && msgno == s->want)
        s->found = 1;
    printf("    msgno=%5d x'%04X' len=%3d byte0=x'%02X' |", msgno,
           (unsigned) (unsigned short) msgno, len,
           len > 0 ? (unsigned) (unsigned char) text[0] : 0u);
    for (i = 0; i < n; i++)
        putchar(text[i] >= ' ' ? text[i] : '.');
    printf("|\n");
}

static int run(const char *title, int want, const char *cmd)
{
    SEEN    s   = { 0, want, 0 };
    int     rc;

    printf("%s\n  %s\n", title, cmd);
    rc = idcams_sysprint(show, &s, "%s", cmd);
    printf("  rc=%d, %d line(s)\n", rc, s.lines);
    CHECK(s.lines > 0, "the callback got lines");
    if (want) {
        char msg[64];
        sprintf(msg, "IDC%04dI came with its number", want);
        CHECK(s.found, msg);
    }
    return rc;
}

int main(int argc, char **argv)
{
    const char  *hlq    = argc > 1 && argv[1][0] ? argv[1] : "IBMUSER";
    char        cmd[200];
    int         rc1, rc;

    printf("TSTIDCSP: idcams_sysprint() on IDCAMS, hlq %s\n", hlq);

    sprintf(cmd, " DELETE '%s.TSTIDCSP.NOSUCH'", hlq);
    rc1 = run("1. DELETE of a missing entry", 3012, cmd);
    CHECK(rc1 == 8, "case 1: rc 8");

    sprintf(cmd, " ALTER '%s.TSTIDCSP.NOSUCH' NEWNAME('%s.TSTIDCSP.OTHER')", hlq, hlq);
    rc = run("2. ALTER NEWNAME of a missing entry", 3012, cmd);
    CHECK(rc == 8, "case 2: rc 8");

    sprintf(cmd, " ALTER '%s.TSTIDCSP.NOSUCH' NEWNAME('%s.TSTIDCSP.NINECHARS')", hlq, hlq);
    rc = run("3. ALTER NEWNAME with a 9-character qualifier", 3203, cmd);
    CHECK(rc == 12, "case 3: rc 12");

    sprintf(cmd, " LISTCAT LEVEL(%s.TSTIDCSP)", hlq);
    rc = run("4. LISTCAT of an empty level", 0, cmd);
    CHECK(rc == 0 || rc == 4, "case 4: rc 0 or 4");

    printf("5. idcams() on case 1, no callback\n");
    sprintf(cmd, " DELETE '%s.TSTIDCSP.NOSUCH'", hlq);
    rc = idcams("%s", cmd);
    printf("  rc=%d\n", rc);
    CHECK(rc == rc1, "idcams() returns what idcams_sysprint() returned");

    printf("TSTIDCSP: %s\n", fails ? "FAILED" : "passed");
    return fails ? 8 : 0;
}
