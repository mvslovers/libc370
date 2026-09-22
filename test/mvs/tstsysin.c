/*
 * tstsysin.c - libc370 #184: a second concurrent open of a spool SYSIN.
 *
 * @@start opens dd:SYSIN as stdin, so every user fopen("dd:SYSIN","r") is
 * inherently a SECOND open.  On an instream DD that is the one thing JES2
 * refuses -- HASPSSSM's SSI open path, reached by SYSIN and SYSOUT alike:
 *
 *     HO300    L     R0,SDBDEB        GET SDB'S DEB POINTER.
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
 * (1) and (5) run in both.  Run BOTH steps; either alone is half a test.
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

        printf("(3) and it is only CONCURRENCY - close first and it opens\n");
        fclose(stdin);
        f = fopen("dd:SYSIN", "r");
        CHECK(f != NULL, "(3) fopen() succeeds once no DCB is held");
        if (f) {
            CHECK(readlines(f) > 0, "(3) and it reads from the top, not EOF");
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
            CHECK(readlines(f) > 0, "(4) and the second stream reads it");
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

    printf("\n=== TSTSYSIN: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");

    return mbt_failed ? 1 : 0;
}
