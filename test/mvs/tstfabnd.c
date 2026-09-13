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
 *       printed.  It then RESCUES the FILE with __fabandon() - partly so the
 *       step does not end S0D37 (@@exit.c fcloses every survivor in
 *       grt->grtfile with no ESTAE around it), partly because @@ACLOSE was
 *       already ENTERED there, so the rescue measures abandon on a
 *       HALF-closed DCB: the state a consumer whose own fclose() abended is
 *       actually in.
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
 * NOT YET RUN ON A TARGET.
 *
 * RC: 0 = every check passed, 8 = at least one did not (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <clibtry.h>
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
    return abend;
}

/* Can this address space still get rid of the data set?  Two probes, because
 * the issue measured both: IDCAMS DELETE (what remove() issues) answered 8
 * from inside, and __dsfree() of the leftover DD answered 4. */
static int scratch(const char *dsn)
{
    int rc = remove(dsn);

    printf("      remove(\"%s\") rc=%d\n", dsn, rc);
    return rc;
}

/* ------------------------------------------------------------------------ */
static void check(int cond, const char *what)
{
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

int main(int argc, char **argv)
{
    char    base[40];
    char    dsng[48];
    char    dsnr[48];
    int     abend;
    int     rc;

    strcpy(base, (argc > 1 && argv[1][0]) ? argv[1] : "IBMUSER.TSTFABND");
    sprintf(dsng, "%s.G", base);
    sprintf(dsnr, "%s.R", base);

    printf("=== tstfabnd: #168 - close a FILE whose last write failed ===\n");
    printf("    green control %s\n", dsng);
    printf("    red   control %s\n\n", dsnr);

    /* ---- (1) GREEN: __fabandon() ------------------------------------- */
    printf("(1) __fabandon() after a D37\n");
    rc = create(dsng);
    if (rc) {
        printf("      __dsalcf rc=%d - CANNOT MEASURE\n", rc);
        unmeasured++;
    }
    else {
        abend = fill(dsng);
        if (abend == 0) {
            printf("      no abend: TRK(1,0) took %ld records."
                   "  CANNOT MEASURE\n", written);
            unmeasured++;
            fclose(thefp);
        }
        else if (abend < 0) {
            unmeasured++;
        }
        else {
            rc = __fabandon(thefp);
            printf("      __fabandon rc=%d (0x%08X)\n", rc, rc);
            check(rc == 0,
                  "(1) __fabandon: CLOSE completed and the DD went");
            if (rc > 0) {
                printf("      CLOSE ABENDED WITH NOTHING PENDING - this is"
                       " the measurement #168 was missing:\n"
                       "      point 3 needs more than try(), the DD cannot"
                       " go while CLOSE fails.\n");
            }
            check(scratch(dsng) == 0,
                  "(1) the data set can be scratched from this address space");
        }
    }

    /* ---- (2) RED CONTROL: fclose() ----------------------------------- */
    printf("\n(2) fclose() after a D37 - the defect, as a control\n");
    rc = create(dsnr);
    if (rc) {
        printf("      __dsalcf rc=%d - CANNOT MEASURE\n", rc);
        unmeasured++;
    }
    else {
        abend = fill(dsnr);
        if (abend <= 0) {
            printf("      no D37 - CANNOT MEASURE\n");
            unmeasured++;
            if (abend == 0) fclose(thefp);
        }
        else {
            char    ddsave[9];

            strcpy(ddsave, thefp->ddname);
            rc = try(closer, thefp);
            printf("      fclose() under try() rc=0x%08X\n", rc);
            check(rc != 0, "(2) fclose() abends - the defect reproduces");

            rc = scratch(dsnr);
            check(rc != 0, "(2) ... and the data set cannot be scratched");

            rc = __dsfree(ddsave);
            printf("      __dsfree(\"%s\") rc=%d\n", ddsave, rc);
            check(rc != 0, "(2) ... and the leftover DD will not unallocate");

            /* The FILE the abended fclose() left standing MUST be taken off
               grt->grtfile before main() returns: @@exit.c walks that array
               and fclose()s every survivor, with no ESTAE around it, so the
               step would end S0D37 at termination and the COND CODE would
               stop being the verdict.
               It also measures something the green case cannot.  @@ACLOSE
               was ENTERED in (2) - FIXWRITE abended before its FREEMAINs and
               before the CLOSE - so this is abandon applied to a HALF-closed
               DCB, not a fresh one, which is the state a consumer whose own
               fclose() abended is actually in. */
            rc = __fabandon(thefp);
            printf("      rescue: __fabandon after the failed fclose"
                   " rc=%d (0x%08X)\n", rc, rc);
            check(rc == 0,
                  "(2) __fabandon rescues a FILE whose fclose() abended");
            printf("      remove(\"%s\") after the rescue rc=%d\n",
                   dsnr, remove(dsnr));
        }
    }

    printf("\n=== tstfabnd: %d check(s) failed, %d case(s) could not"
           " measure ===\n", bad, unmeasured);

    return (bad || unmeasured) ? 8 : 0;
}
