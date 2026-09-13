/*
 * tstfabnd.c - libc370 #168: __fabandon() on the target, measured.
 *
 * The host test (test/host/tstfabnd.c) pins the SHAPE of the call: that
 * abandon discards instead of flushing, that it tells the DCB there is
 * nothing pending, that CLOSE goes through ___try(), and what each return
 * code means.  Three things it cannot answer, and this probe exists for
 * exactly those:
 *
 *   - does CLOSE with nothing pending COMPLETE on a data set that is out of
 *     space?  BSAM CLOSE writes the EOF mark and can reach EOV, so it may
 *     abend on its own.  This was NOT measured when #168 was written; the
 *     issue assumed it away and the run below is what settles it.
 *   - does the DD then really go, so the data set can be scratched from the
 *     SAME address space?  That is the whole point of #168: mvslovers/ftpd
 *     STORs into a data set with DISP=(NEW,CATLG,DELETE) and is meant to
 *     scratch the partial on an out-of-space condition (ftpd#129).
 *   - does the defect still reproduce, i.e. is the control still red?
 *
 * SHAPE.  Both cases are mvslovers/ftpd's: __dsalcf() creates the data set
 * with SPACE=TRK(1,0) - no secondary, so the first overflow is a D37 and not
 * an extend - __dsfree() catalogs it, and fopen(dsn,"wb") reopens it by name.
 * fwrite() then runs until the D37, under try(), exactly the way ftpd's ESTAE
 * recovers a STOR.
 *
 * CASES
 *   (1) GREEN.  After the D37, __fabandon().  Then remove() - IDCAMS DELETE,
 *       from inside the address space that just hit the abend - must answer
 *       0.  That is the measured symptom from the issue, turned around.
 *   (2) RED CONTROL.  The same up to the D37, then fclose() under try().  It
 *       is expected to abend a second time and leave the DD allocated;
 *       remove() then answers nonzero and __dsfree(ddname) answers 4.  If
 *       this case ever goes quiet the premise of #168 has changed and the
 *       green case above stops proving anything - so it is checked, not just
 *       printed.  The defect is measured with __dsfree() - ftpd's "FREE
 *       RC=4" - and NOT with remove(): IDCAMS DELETE escalates the SYSDSN
 *       ENQ to exclusive for the rest of the step (#127), which breaks the
 *       rescue below for a reason that has nothing to do with #168.
 *       It then RESCUES the FILE with __fabandon() - partly so the step does
 *       not end in a teardown abend (@@exit.c fcloses every survivor in
 *       grt->grtfile with no ESTAE around it, and MVS closes any DCB the
 *       task left open at termination), partly because @@ACLOSE was already
 *       ENTERED there, so the rescue measures abandon on a HALF-closed DCB:
 *       the state a consumer whose own fclose() abended is actually in.
 *   (3) WHICH HALF of fclose() takes the second abend.  #168 assumed a second
 *       D37 from the re-driven write; the target says otherwise.  fflush()
 *       and __aclose() are driven under separate try()s on a fresh data set,
 *       so the run says it rather than the issue assuming it.
 *
 * That the data set can be deleted from ANOTHER address space was already
 * measured in the issue on 2026-09-09; this probe does not re-measure it.
 * jcl/tstfabnd.jcl still carries a SCRATCH step, now only as a net.
 *
 * SETUP: none.  Neither work data set may exist when the test starts.
 *
 * PARM='<base>'  (default IBMUSER.TSTFABND) - the cases append .G and .R
 *
 * UNIT=SYSDA below is the esoteric name on the reference system - change it
 * if yours differs.
 *
 * BUILD (host).  -L build/sdk is LOAD-BEARING: without it -lc autocalls the
 * INSTALLED sysroot libc370, which has no __fabandon() at all and the link
 * fails - which is at least loud.  Check the module really took it:
 *
 *     make build
 *     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstfabnd.c \
 *           -o TSTFABND -flinker-output=iebcopy
 *     python3 -c "print(open('TSTFABND','rb').read().count( \
 *                       '@@ADISC'.encode('cp037')))"        # must be > 0
 *     ld370 --pack TSTFABND=TSTFABND.iebcopy -o probe -xmit \
 *           --dsn IBMUSER.LIBC370.ABNSCR
 *
 * Then upload probe.xmit to IBMUSER.MBT.XMIT.IN, run jcl/recvfabn.jcl, run
 * jcl/tstfabnd.jcl.
 *
 * MEASURED 2026-09-13 on mvsdev, JOB00245.  Job CC 0000: all three steps
 * green, 7/7 checks PASS, no teardown abend.  TRK(1,0) on WORK00 takes 200
 * records of 80 before the D37.
 *
 *   GREEN   __fabandon() rc=0        remove() rc=0
 *   SPLIT   fflush() alone rc=0x000C4000   __aclose() alone rc=0, remove() 0
 *   FCLOSE  fclose() alone rc=0x000C4000   __dsfree() rc=4
 *
 * Three things the run settled, none of them assumed:
 *
 *  - CLOSE WITH NOTHING PENDING COMPLETES on a data set that is out of space.
 *    That was the open question in #168 - BSAM CLOSE writes the EOF mark and
 *    can reach EOV - and the answer is that it does not abend.  try() around
 *    it stays, but point 3 did not fire on this system.
 *  - POINT 1 IS THE CRUX, on the target and not just by reading the source:
 *    SPLIT drives the two halves of fclose() separately and it is the FLUSH
 *    that abends, while CLOSE straight afterwards is clean and the DD goes.
 *  - THE SECOND ABEND IS A PROGRAM CHECK, NOT A SECOND D37.  0x0C4 and 0x0C6
 *    both appeared across runs for identical code, so libc370 re-driving a
 *    WRITE against a DCB that has taken an x37 does not fail cleanly, it
 *    walks into wild storage.  #168 assumed a second D37; it is worse than
 *    that, and it is another reason the flush must not be re-driven.
 *
 * ONE CASE PER STEP, and that is not cosmetic.  Run together, the FCLOSE
 * rescue answered -2 and left the DD (JOB00231/233/241); run in its own
 * address space it answers 0 (JOB00235/245).  Every -2 had BOTH an earlier
 * IDCAMS DELETE and an earlier D37 in the same step, and nothing measured
 * separates the two - candidates are the escalated SYSDSN ENQ of #127 and a
 * DEB the wild store left chained (IEC999I IFG0TC0A named the same address,
 * 9BB9AC, on four consecutive runs).  It is NOT established, it is NOT
 * asserted here, and splitting the steps takes it out of the probe.
 *
 * RC: 0 = every check passed, 8 = at least one did not (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <clibtry.h>
#include <clibwto.h>
#include <mvssupa.h>   /* __aclose() - (3) drives it without fclose() */
#include "clibio.h"     /* __dsalcf(), __dsfree(), __fabandon() */

#define CREATE  "DSN=%s;DISP=(NEW,CATLG,DELETE);DSORG=PS;RECFM=FB;"          \
                "LRECL=80;BLKSIZE=800;UNIT=SYSDA;SPACE=TRK(1,0)"

#define LIMIT   20000           /* records; one track holds a few hundred   */

static int  bad = 0;
static int  unmeasured = 0;     /* a case that HAD to measure and did not   */
static char dd[9];

static FILE *thefp;             /* the FILE the try()'d helpers work on     */
static long  written;

/* ------------------------------------------------------------------------ *
 * The two things that run under ESTAE.  Neither returns after the abend -
 * try() retries past them - so everything they report goes through statics.
 * ------------------------------------------------------------------------ */
static void writer(FILE *fp)
{
    char    rec[80];

    memset(rec, 'X', sizeof(rec));
    for (written = 0; written < LIMIT; written++) {
        if (fwrite(rec, 1, sizeof(rec), fp) != sizeof(rec)) break;
    }
}

static void closer(FILE *fp)
{
    fclose(fp);
}

/* fclose()'s two halves, so the RED step can say which one takes the second
 * abend: the flush re-driving the block that already failed (point 1 of
 * #168), or CLOSE itself (point 3). */
static void flusher(FILE *fp)
{
    fflush(fp);
}

static void acloser(FILE *fp)
{
    __aclose(fp->dcb);
}

/* ------------------------------------------------------------------------ */
static int create(const char *dsn)
{
    int rc = __dsalcf(dd, CREATE, dsn);

    if (rc == 0) __dsfree(dd);          /* the free catalogs it - ftpd step 1 */
    return rc;
}

/* Open the data set by name and write into it until it abends.  Returns the
 * try() code: 0x00D37000 is the D37 this test is built around, 0 means the
 * writes all fit and there is nothing to abandon. */
static int fill(const char *dsn)
{
    char    fname[52];
    int     abend;

    sprintf(fname, "'%s'", dsn);
    thefp = fopen(fname, "wb");
    if (!thefp) {
        printf("      fopen(\"%s\",\"wb\") returned NULL\n", fname);
        return -1;
    }
    printf("      fopen ok, ddname=%.8s\n", thefp->ddname);

    abend = try(writer, thefp);
    printf("      %ld record(s) written, try() rc=0x%08X\n", written, abend);
    wtof("TSTFABND write dd=%.8s recs=%ld try=%08X",
         thefp->ddname, written, abend);
    return abend;
}

/* Can this address space still get rid of the data set?  Two probes, because
 * the issue measured both: IDCAMS DELETE (what remove() issues) answered 8
 * from inside, and __dsfree() of the leftover DD answered 4. */
static int scratch(const char *dsn)
{
    int rc = remove(dsn);

    printf("      remove(\"%s\") rc=%d\n", dsn, rc);
    wtof("TSTFABND remove %s rc=%d", dsn, rc);
    return rc;
}

/* Create the data set, open it and write into it until the D37.  Returns 0
 * when there is a FILE in thefp with a failed write behind it, nonzero when
 * the case could not be SET UP - which is NOT a failure of the thing under
 * test, and is counted as unmeasured rather than red. */
static int setup(const char *dsn, int *abend)
{
    int rc = create(dsn);

    if (rc) {
        printf("      __dsalcf rc=%d - CANNOT MEASURE\n", rc);
        unmeasured++;
        return 1;
    }
    *abend = fill(dsn);
    if (*abend == 0) {
        printf("      no abend: TRK(1,0) took %ld records."
               "  CANNOT MEASURE\n", written);
        unmeasured++;
        fclose(thefp);
        return 1;
    }
    if (*abend < 0) {
        unmeasured++;
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Every verdict goes to the console as well as to SYSPRINT.  This probe
 * takes three D37s on purpose, and SYSOUT records sit in the QSAM block
 * buffer until fclose: an abend anywhere - including in the step's own
 * teardown - discards every line printed so far.  WTOs land in the JES2 job
 * log immediately and survive it.  printf is the human transcript; the WTOs
 * are the record. */
static void check(int cond, const char *what)
{
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", what);
    wtof("TSTFABND %s %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

int main(int argc, char **argv)
{
    char    base[40];
    char    dsn[48];
    char    ddsave[9];
    char    mode;
    int     abend;
    int     rc;

    /* toupper(), never an arithmetic fold: in EBCDIC 'a' is 0x81 and 'A' is
       0xC1, so "c >= 'a'" is true for every uppercase letter too and the
       classic ASCII fold turns 'G' into 0x07.  Measured: mvsdev JOB00243,
       all three steps fell through to the default. */
    mode = (argc > 1 && argv[1][0]) ? (char)toupper((unsigned char)argv[1][0])
                                    : 'G';
    strcpy(base, (argc > 2 && argv[2][0]) ? argv[2] : "IBMUSER.TSTFABND");

    printf("=== tstfabnd: #168 - close a FILE whose last write failed ===\n");

    switch (mode) {

    /* ---- GREEN: __fabandon() instead of fclose() --------------------- */
    case 'G':
        sprintf(dsn, "%s.G", base);
        printf("    step GREEN - the fix\n\n");
        printf("(1) __fabandon() after a D37 on %s\n", dsn);

        if (setup(dsn, &abend)) break;

        rc = __fabandon(thefp);
        printf("      __fabandon rc=%d (0x%08X)\n", rc, rc);
        wtof("TSTFABND (1) __fabandon rc=%d (%08X)", rc, rc);
        check(rc == 0, "(1) __fabandon: CLOSE completed and the DD went");
        if (rc > 0) {
            printf("      CLOSE ABENDED WITH NOTHING PENDING - point 3 of"
                   " #168 needs more than try().\n");
        }
        check(scratch(dsn) == 0,
              "(1) the data set can be scratched from this address space");
        break;

    /* ---- SPLIT: which half of fclose() abends ------------------------ */
    case 'S':
        sprintf(dsn, "%s.S", base);
        printf("    step SPLIT - which half of fclose() abends\n\n");
        printf("(2) fflush() and __aclose() under separate try()s on %s\n",
               dsn);

        if (setup(dsn, &abend)) break;

        rc = try(flusher, thefp);
        printf("      fflush() alone   rc=0x%08X\n", rc);
        wtof("TSTFABND (2) fflush alone rc=%08X", rc);
        check(rc != 0, "(2) the FLUSH is what abends - point 1 is the crux");

        abend = try(acloser, thefp);
        printf("      __aclose() alone rc=0x%08X\n", abend);
        wtof("TSTFABND (2) aclose alone rc=%08X", abend);
        check(abend == 0,
              "(2) CLOSE alone completes - even after the flush failed");

        /* Tear down, or @@exit.c fcloses this FILE at termination:
           fp->upto still points past the block the flush could not write,
           so it would re-drive the same program check with no ESTAE.
           NOT __fabandon() here - acloser() above already CLOSEd this DCB,
           and @@ACLOSE frees the buffers and the work area, so a second one
           FREEMAINs storage that is gone (S0A0A, mvsdev JOB00237: a defect
           in this probe, not in the library).  Dropping _FILE_FLAG_OPEN
           makes fclose() skip both the flush and the close and run the
           teardown only, which is all that is left to do. */
        thefp->flags &= ~_FILE_FLAG_OPEN;
        fclose(thefp);
        printf("      teardown only (the DCB is already closed)\n");
        check(scratch(dsn) == 0,
              "(2) ... and the DD went, so the data set can be scratched");
        break;

    /* ---- FCLOSE: the defect, as a control ---------------------------- */
    case 'F':
        sprintf(dsn, "%s.R", base);
        printf("    step FCLOSE - the defect, as a control\n\n");
        printf("(3) fclose() after a D37 on %s\n", dsn);

        if (setup(dsn, &abend)) break;

        strcpy(ddsave, thefp->ddname);
        rc = try(closer, thefp);
        printf("      fclose() under try() rc=0x%08X\n", rc);
        wtof("TSTFABND (3) fclose under try rc=%08X", rc);
        check(rc != 0, "(3) fclose() abends - the defect reproduces");

        /* __dsfree(), NOT remove().  IDCAMS DELETE demands the data set
           exclusively; __dsfree() is a plain SVC 99 unallocate and is the
           exact probe ftpd logs as "FREE RC=4". */
        rc = __dsfree(ddsave);
        printf("      __dsfree(\"%s\") rc=%d\n", ddsave, rc);
        wtof("TSTFABND (3) __dsfree %.8s rc=%d", ddsave, rc);
        check(rc != 0, "(3) ... and the leftover DD will not unallocate");

        /* Can abandon rescue it AFTER the fact?  REPORTED, NOT ASSERTED.
           It works HERE only because of what the SPLIT step measured: this
           fclose() died in the FLUSH, so the DCB was still intact when
           __fabandon() reached it.  Had it died inside CLOSE - point 3, the
           half that did not fire on this system - __adisc() would write into
           a work area @@ACLOSE had already FREEMAINed and try(__aclose)
           would free it twice.  That is the S0A0A the SPLIT step's comment
           records, and the second reason the supported use is __fabandon()
           INSTEAD of fclose(), which the GREEN step measures.
           It has also been seen to answer -2 and leave the DD: every such
           run had BOTH an earlier IDCAMS DELETE and an earlier D37 in the
           same step, and nothing here separates the two.  Candidates, not a
           cause: the escalated SYSDSN ENQ of #127, or a DEB the re-driven
           flush's wild store left chained (IEC999I IFG0TC0A named the same
           address, 9BB9AC, on four consecutive runs).  Splitting the cases
           into one step each removes the confound from the probe; the open
           question is recorded in TODO.md, not answered here. */
        rc = __fabandon(thefp);
        printf("      rescue: __fabandon after the failed fclose rc=%d"
               " (0x%08X)\n", rc, rc);
        wtof("TSTFABND (3) rescue __fabandon rc=%d (%08X)", rc, rc);
        printf("      remove(\"%s\") rc=%d - NOT a verdict, see the source\n",
               dsn, remove(dsn));
        break;

    default:
        printf("    unknown PARM \"%s\" - use GREEN, SPLIT or FCLOSE\n",
               argv[1]);
        unmeasured++;
        break;
    }

    printf("\n=== tstfabnd: %d check(s) failed, %d case(s) could not"
           " measure ===\n", bad, unmeasured);
    wtof("TSTFABND VERDICT %c failed=%d unmeasured=%d", mode, bad, unmeasured);

    return (bad || unmeasured) ? 8 : 0;
}
