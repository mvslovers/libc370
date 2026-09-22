/*
 * tstsysin.c - libc370 #184: a second concurrent open of a spool SYSIN.
 *
 * @@start opens dd:SYSIN as stdin, so every user fopen("dd:SYSIN","r") is
 * inherently a SECOND open.  On an instream DD that is the one thing JES2
 * refuses.  HOSOPEN dispatches by data set TYPE -- HO000 internal reader,
 * HO100 'SI', HO200 'SO', HO300 'PS' -- and an instream SYSIN reaches
 * HO100, whose non-XBM path falls through HO107 into the process-SYSOUT
 * open code.  Plain SYSOUT never comes here; HO200 is its own block and
 * keeps an open count:
 *
 *     HO300    DS    0H
 *              L     R0,SDBDEB        GET SDB'S DEB POINTER.
 *              LTR   R0,R0            IF NO DEB, DATA SET IS
 *              BZ    HO110            CLOSED.  GO OPEN IT.
 *              TM    SJBFLG1,SJB1XBM  IF OPEN ALREADY AND XBM,
 *              BO    HORET            IGNORE OPEN.
 *              B     HOERR            NOT XBM BUT OPEN - ERROR.
 *
 * and the S013-C0 follows.  It cannot be made to work, so libc370 refuses
 * it at the call instead: fopen() answers NULL + EBUSY and the program
 * survives to handle it.
 *
 * MVS target only, and not merely because of EBCDIC: the whole subject is
 * a JES2 spool data set, which a host has no equivalent of.
 *
 * TWO STEPS, one member.  The cases that can run depend on what SYSIN is,
 * and the test reads the TIOT to find out rather than being told:
 *   SYSIN DD *          -> (2) the refusal, (3) reopen after close
 *   SYSIN DD DSN=...    -> (4) a real data set still opens twice
 * (1), (5) and (6) run in both.  Run BOTH steps; either alone is half
 * a test -- and (4) is the half that catches an over-refusal.
 *
 * Build:   cc370 -O1 -Iinclude -L build/sdk test/mvs/tstsysin.c \
 *                -o TSTSYSIN -flinker-output=iebcopy
 *          ld370 --pack TSTSYSIN=TSTSYSIN.iebcopy -o tstsysin -xmit \
 *                --dsn IBMUSER.LIBC370.TEST.LINKLIB
 * Install: RECEIVE the XMIT into the STEPLIB of jcl/tstsysin.jcl.
 *          -L build/sdk is not optional: without it the link takes the
 *          installed sysroot libc, which has no @@DDBUSY and would abend
 *          the probe for a reason that has nothing to do with the test.
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 *
 * Run:     mvsdev JOB00438, CC 0000, 2026-09-22 -- 13/13 in the SPOOL
 *          step and 8/8 in REALDS (different steps run different cases).
 *
 * Proven red by the cleanest control there is: THE SAME SOURCE linked
 * against the installed pre-fix libc, which has no @@DDBUSY.  The SPOOL
 * step abends before its first check --
 *
 *     IEC141I 013-C0,IGG0199G,TSTSYSOL,SPOOL,SYSIN
 *     IEF450I TSTSYSOL SPOOL - ABEND S013 U0000
 *
 * -- which is issue #184 verbatim (JOB00439).  So the green run is not a
 * test that happens to pass; it is the same program surviving what used
 * to kill it.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <clibwto.h>
#include <clibdsab.h>
#include <ieftiot.h>

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }         \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg));           \
                    wtof("TSTSYSIN FAIL: %s", (msg)); }                    \
    } while (0)

/* the flag byte of a DD's TIOT entry, or -1 when there is no such DD */
static int ddflags(const char *ddname)
{
    DSAB    *dsab = get_dsab(0, ddname);
    TIOTDD  *e;

    if (!dsab) return -1;
    e = dsab->dsabtiot;
    if (!e) return -1;
    return (unsigned char)e->TIOELINK;
}

static int readlines(FILE *f)
{
    char b[133];
    int  n = 0;
    while (fgets(b, sizeof b, f)) n++;
    return n;
}

int main(void)
{
    int   sysin  = ddflags("SYSIN");
    int   sysprt = ddflags("SYSPRINT");
    int   spool;
    FILE *f;
    char  line[133];

    printf("=== TSTSYSIN: libc370 #184 spool SYSIN reopen ===\n\n");
    printf("SYSIN TIOELINK=%02X  SYSPRINT TIOELINK=%02X\n", sysin, sysprt);

    CHECK(sysin >= 0, "(1) the SYSIN DD has a TIOT entry");
    CHECK(sysprt >= 0, "(1) the SYSPRINT DD has a TIOT entry");

    /* The discriminator the fix is built on.  ieftiot.h's comment points
       at TIOESYIN (X'04') for "spooled SYSIN" and that bit is never set
       on 3.8j; the bit that marks a spool DD is TIOESSDS (X'02'), the VS2
       meaning of the same byte.  A check written from the comment would
       compile, run and never fire -- so assert the shape of the world the
       fix assumes, here, where it fails loudly instead. */
    CHECK(sysprt >= 0 && (sysprt & TIOESSDS),
          "(1) SYSOUT=* carries TIOESSDS (X'02')");
    /* Informational, not a guard: since the check masks TIOESYIN|TIOESSDS
       it cannot silently never fire even if X'04' turns up.  This arm
       records what the system actually does, and it carries weight only
       in the SPOOL step -- in REALDS, SYSIN is X'00' and it asserts
       nothing. */
    CHECK(sysin < 0 || !(sysin & TIOESYIN),
          "(1) TIOESYIN (X'04') is NOT the spool marker on this system");

    spool = (sysin >= 0) && (sysin & TIOESSDS);
    printf("%s\n", spool ? "SYSIN is a spool (instream) data set"
                         : "SYSIN is a real data set");

    if (spool) {
        printf("(2) the second concurrent open is refused, not fatal\n");
        errno = 0;
        f = fopen("dd:SYSIN", "r");
        /* reaching this line at all is most of the point: before the fix
           the OPEN abended S013-C0 and there was no next line */
        CHECK(f == NULL, "(2) fopen() returns NULL instead of abending");
        CHECK(errno == EBUSY, "(2) errno is EBUSY");
        if (f) fclose(f);

        /* A backward fseek CAN route through __reopen(), which calls
           fopen() while the old FILE is still open and registered -- so
           the refusal would surface there too.  It does not happen for a
           small instream SYSIN, and this is the check that says so:
           @@fseek.c satisfies a seek whose target is still inside the
           current buffer without reopening anything, and one block of
           instream data is entirely inside it.  Measured, after this
           check went red asserting the opposite.
           What a seek PAST the buffer does on a spool SYSIN is not
           measured; it needs more than one block of instream data. */
        printf("(2) a backward seek inside the buffer does not reopen\n");
        CHECK(fgets(line, sizeof line, stdin) != NULL,
              "(2) stdin still reads normally after the refusal");
        CHECK(fseek(stdin, 0L, SEEK_SET) == 0,
              "(2) @@fseek satisfies an in-buffer backward seek, no reopen");
        CHECK(ferror(stdin) == 0,
              "(2) @@fseek leaves the stream unerrored");

        printf("(3) and it is only CONCURRENCY - close first and it opens\n");
        fclose(stdin);
        /* the GRT slot is not cleared by fclose() and @@exit.c compares
           against it, so model the workaround the way a porter should */
        stdin = NULL;
        f = fopen("dd:SYSIN", "r");
        CHECK(f != NULL, "(3) fopen() succeeds once no DCB is held");
        if (f) {
            CHECK(readlines(f) == 1,
                  "(3) and it reads the 1 line from the top, not EOF");
            fclose(f);
        }
        else {
            CHECK(0, "(3) and it reads from the top, not EOF [skipped]");
        }
    }
    else {
        printf("(4) a real data set still tolerates two concurrent DCBs\n");
        errno = 0;
        f = fopen("dd:SYSIN", "r");
        CHECK(f != NULL, "(4) fopen() of a real SYSIN still succeeds");
        if (f) {
            CHECK(readlines(f) == 1,
                  "(4) and the second stream reads its 1 line");
            fclose(f);
        }
        else {
            CHECK(0, "(4) and the second stream reads it [skipped]");
        }
    }

    /* The over-refusal guard, and it runs in both steps.  SYSOUT reaches
       HASPSSSM HO200, which keeps an open count and returns happily on
       the second open, so a doubled SYSPRINT works and must keep working.
       If the refusal ever widens to output, this is what says so. */
    printf("(5) SYSOUT is NOT refused - it keeps an open count in JES2\n");
    errno = 0;
    f = fopen("dd:SYSPRINT", "w");
    CHECK(f != NULL, "(5) a second concurrent SYSPRINT open still succeeds");
    if (f) {
        fputs("TSTSYSIN wrote this through a second SYSPRINT stream\n", f);
        fclose(f);
    }

    /* (6) The over-refusal guard, and the reason the check tests the
       HOLDER's direction rather than the caller's mode.  JES2 dispatches
       on DSNDSTYP -- the data set TYPE -- so an 'SO' data set opened for
       READ still goes to HO200 and is allowed.  Measured on the pre-fix
       library: this very call SUCCEEDS (JOB00429).  A check keyed on the
       incoming mode alone refuses it and breaks working code. */
    printf("(6) a SYSOUT DD opened for READ is not refused either\n");
    /* SYSTERM, deliberately, NOT SYSPRINT.  HO200/HODEBACB points the
       second DEB and ACB at the same SDB the holder is writing, so a read
       stream on SYSPRINT can move the position under this test's own
       output and corrupt the evidence for every other case.  SYSTERM is
       the same 'SO' type, is held for write by stderr, and carries
       nothing the test needs -- wtof() is WTO, not stderr.
       Open and close only: the OPEN succeeding is what was measured
       (JOB00429); what a READ of a spool SYSOUT returns is not, so
       nothing here asserts it.  The gate on the close is the step's own
       condition code. */
    errno = 0;
    f = fopen("dd:SYSTERM", "r");
    CHECK(f != NULL, "(6) read open of a held SYSOUT DD still succeeds");
    if (f) fclose(f);

    printf("\n=== TSTSYSIN: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");

    return mbt_failed ? 1 : 0;
}
