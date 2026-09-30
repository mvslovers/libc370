/*
 * tstrfree.c - libc370 #229: rclose() frees the DD ropen() allocated,
 *              and #231: ropen() of a quoted name without a member.
 *
 * ropen() on a data set name allocates a DD with __fildef() (SVC 99,
 * DALRTDDN) and records it in the handle - fp->dyn, fp->ddname.  rclose()
 * never read either, so every ropen()/rclose() pair left the allocation
 * behind until step end.  ropen() had the same hole on its own error path:
 * when __aopen() failed after the allocation, the DD stayed.
 *
 * THE MEASURE is the step's DSAB chain: get_dsab(NULL, <dd>) finds a DD
 * that is still allocated, and the chain length counts them.  Checks (1)
 * and (2) show on this system that both move with an SVC 99 allocate and
 * unallocate before anything is concluded from them.
 *
 * CHECKS
 *   (1) control: __fildef() allocates, the chain grows by one and holds
 *       the returned DD
 *   (2) control: __fdclr() unallocates, the chain is back to its length
 *       and the DD is gone
 *   (3) ropen(<ps>, write) by name succeeds and its DD is on the chain
 *   (4) rwrite() + rclose() answer 0
 *   (5) after rclose() the DD is gone                         - THE FIX
 *   (6) after rclose() the chain is back to its length        - THE FIX
 *   (7) LOOP ropen()/rread()/rclose() cycles read the record back
 *   (8) after the cycles the chain is back to its length      - THE FIX
 *   (9) control: ropen(<pds>(NOSUCH)) fails - __aopen() finds no member
 *  (10) after that failure the chain is back to its length    - THE FIX
 *  (11) ropen('<ps>', read) - quoted, no member - succeeds and reads the
 *       record back                                       - THE FIX #231
 *  (12) after its rclose() the chain is back to its length (checked only
 *       when (11) opened: a failed allocation leaves nothing to count)
 *
 * #231: ropen() dropped the opening quote of a quoted name but copied the
 * closing one into the DSN when no member followed, so the allocation
 * failed with SVC 99 X'035C' (errno 860, mvsdev JOB00766).  With a member
 * the copy stopped at '(' first, which is why (9) always allocated.
 * Against a sysroot that has #229 but not #231, (11) is the only FAIL:
 * mvsdev JOB00789, step RED rc=12 errno=860, step GREEN 12/12.
 *
 * RED before the fix: (5), (6), (8) and (10) fail - the chain grows by one
 * per ropen() by name, the failed one included.  Nothing abends.  mvsdev
 * JOB00768: step RED (installed sysroot) 6 -> 28 DSABs over 22 opens,
 * step GREEN (the fix) 10/10, the chain stays at 6.
 *
 * SETUP: jcl/tstrfree.jcl creates <prefix>.PS (FB 80/800: on a V data set
 * __awrite() takes the record with its RDW and abends 002 when the RDW
 * does not match the length, @@AWRITE WRITENEW - JOB00767) and an empty
 * <prefix>.PDS in a step of its own, so neither is allocated to the probe
 * step when it starts, and scratches both afterwards.
 *
 * PARM='<prefix>'   (default IBMUSER.TSTRFREE)
 *
 * BUILD (host).  -L build/sdk is LOAD-BEARING - it links the branch libc,
 * not the installed sysroot:
 *
 *     make build
 *     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstrfree.c \
 *           -o TSTRFREE -flinker-output=iebcopy
 *     ld370 --pack TSTRFREE=TSTRFREE.iebcopy -o probe -xmit \
 *           --dsn IBMUSER.LIBC370.RFRSCR
 *
 * upload probe.xmit to IBMUSER.LIBC370.RFRXMIT and run jcl/tstrfree.jcl,
 * which RECEIVEs it and runs the probe.
 *
 * RC: 0 = every check passed, 8 = at least one did not (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <mvs/wto.h>
#include <clibdsab.h>
#include <mvs/rfile.h>

extern int      __fildef(char *fdddname, char *fnm, int mymode, int type);
extern int      __fdclr(char *fdddname);

#define LOOP    20
#define LRECL   80
#define RECORD  "TSTRFREE #229 RECORD"

static int  bad = 0;

static void check(int cond, const char *what)
{
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", what);
    wtof("TSTRFREE %s %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* Length of this step's DSAB chain, or -1 if it is not a chain of DSABs. */
static int dsabs(void)
{
    DSAB    *ds;
    int     n = 0;

    for (ds = get_dsab(NULL, NULL); ds; ds = ds->dsabfchn) {
        if (memcmp(ds->dsabid, "DSAB", 4) != 0) return -1;
        if (++n > 5000) return -1;
    }
    return n;
}

static int allocated(const char *dd)
{
    return get_dsab(NULL, dd) != NULL;
}

int main(int argc, char **argv)
{
    const char  *prefix = argc > 1 ? argv[1] : "IBMUSER.TSTRFREE";
    char        ps[64], pds[64], name[64];
    char        dd[9];
    char        rec[LRECL];
    char        buf[512];
    RFILE       *rf     = NULL;
    size_t      got;
    int         base, n, rc, rc2, i, ok;

    sprintf(ps,  "%s.PS",  prefix);
    sprintf(pds, "%s.PDS", prefix);
    printf("=== tstrfree: #229 on %s, %s ===\n", ps, pds);

    base = dsabs();
    printf("      DSAB chain at start: %d\n", base);
    wtof("TSTRFREE start dsabs=%d", base);
    if (base < 0) {
        check(0, "the DSAB chain can be walked");
        goto done;
    }

    /* ---- controls: the measure sees an SVC 99 allocate and unallocate */
    memset(dd, 0, sizeof(dd));
    rc = __fildef(dd, ps, 0, 0);
    n  = dsabs();
    printf("      __fildef rc=%d dd=\"%s\" dsabs=%d\n", rc, dd, n);
    wtof("TSTRFREE fildef rc=%d dd=%s dsabs=%d", rc, dd, n);
    check(rc == 0 && dd[0] && n == base + 1 && allocated(dd),
          "(1) control: __fildef() adds one DSAB, holding its DD");

    rc = __fdclr(dd);
    n  = dsabs();
    printf("      __fdclr rc=%d dsabs=%d\n", rc, n);
    wtof("TSTRFREE fdclr rc=%d dsabs=%d", rc, n);
    check(rc == 0 && n == base && !allocated(dd),
          "(2) control: __fdclr() removes it again");

    /* ---- rclose() after a write by name --------------------------------- */
    /* unquoted: in batch no prefix is added; the quoted form is (11) */
    sprintf(name, "%s", ps);
    memset(dd, 0, sizeof(dd));
    rc = ropen(name, 1, &rf);
    if (rc == 0 && rf) {
        memcpy(dd, rf->ddname, 8);
        n = dsabs();
        printf("      ropen(%s, 1) rc=0 dd=\"%s\" dyn=%d dsabs=%d\n",
               name, dd, rf->dyn, n);
        check(rf->dyn && allocated(dd) && n == base + 1,
              "(3) ropen() by name allocates a DD of its own");
        memset(rec, ' ', sizeof(rec));
        memcpy(rec, RECORD, strlen(RECORD));
        rc  = rwrite(rf, rec, sizeof(rec));
        rc2 = rclose(rf);
        n   = dsabs();
        printf("      rwrite rc=%d rclose rc=%d errno=%d dsabs=%d\n",
               rc, rc2, errno, n);
        wtof("TSTRFREE rclose dd=%s rwrite=%d rclose=%d dsabs=%d still=%d",
             dd, rc, rc2, n, allocated(dd));
        check(rc == 0 && rc2 == 0, "(4) rwrite() and rclose() answer 0");
        check(!allocated(dd), "(5) after rclose() the DD is gone");
        check(n == base, "(6) after rclose() the DSAB chain is back");
    }
    else {
        printf("      ropen(%s, 1) rc=%d errno=%d\n", name, rc, errno);
        check(0, "(3) ropen() by name allocates a DD of its own");
    }

    /* ---- LOOP read cycles ----------------------------------------------- */
    for (i = 0, ok = 0; i < LOOP; i++) {
        rf = NULL;
        if (ropen(name, 0, &rf) || !rf) break;
        memset(buf, 0, sizeof(buf));
        got = 0;
        rc  = rread(rf, buf, &got);
        rc2 = rclose(rf);
        if (rc == 0 && rc2 == 0 && got == LRECL
            && memcmp(buf, rec, LRECL) == 0) ok++;
    }
    n = dsabs();
    printf("      %d read cycles, %d read the record back, dsabs=%d\n",
           i, ok, n);
    wtof("TSTRFREE loop cycles=%d ok=%d dsabs=%d", i, ok, n);
    check(ok == LOOP, "(7) every ropen()/rread()/rclose() cycle read it back");
    check(n == base, "(8) after the cycles the DSAB chain is back");

    /* ---- #231: a quoted name without a member ------------------------- */
    sprintf(name, "'%s'", ps);
    rf    = NULL;
    errno = 0;
    rc    = ropen(name, 0, &rf);
    if (rc == 0 && rf) {
        memset(buf, 0, sizeof(buf));
        got = 0;
        rc  = rread(rf, buf, &got);
        rc2 = rclose(rf);
        n   = dsabs();
        printf("      ropen(%s, 0) rc=0 rread rc=%d got=%d rclose rc=%d "
               "dsabs=%d\n", name, rc, (int)got, rc2, n);
        wtof("TSTRFREE quoted rread=%d got=%d rclose=%d dsabs=%d",
             rc, (int)got, rc2, n);
        check(rc == 0 && rc2 == 0 && got == LRECL
              && memcmp(buf, rec, LRECL) == 0,
              "(11) ropen() of a quoted name reads the record back");
        check(n == base, "(12) after its rclose() the DSAB chain is back");
    }
    else {
        n = dsabs();
        printf("      ropen(%s, 0) rc=%d errno=%d dsabs=%d\n",
               name, rc, errno, n);
        wtof("TSTRFREE quoted rc=%d errno=%d dsabs=%d", rc, errno, n);
        check(0, "(11) ropen() of a quoted name reads the record back");
    }

    /* ---- ropen()'s own error path --------------------------------------- */
    sprintf(name, "'%s(NOSUCH)'", pds);
    rf    = NULL;
    errno = 0;
    rc    = ropen(name, 0, &rf);
    n     = dsabs();
    printf("      ropen(%s, 0) rc=%d errno=%d dsabs=%d\n", name, rc, errno, n);
    wtof("TSTRFREE nosuch rc=%d errno=%d dsabs=%d", rc, errno, n);
    check(rc != 0, "(9) control: ropen() of a missing member fails");
    check(n == base, "(10) after that failure the DSAB chain is back");
    if (rc == 0 && rf) rclose(rf);

done:
    printf("\n=== tstrfree: %d check(s) failed ===\n", bad);
    wtof("TSTRFREE VERDICT failed=%d", bad);

    return bad ? 8 : 0;
}
