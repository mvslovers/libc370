/*
 * tstdsnmt.c - libc370 #181: __dsalc() with UNIT= or VOLSER= answers instead
 * of waiting for the operator, measured on MVS.
 *
 * The host test (test/host/tstdsnmt.c) pins that the request block carries
 * S99NOMNT when the opts name a unit or a volume, and only then.  What it
 * cannot answer:
 *
 *   - does SVC 99 then really refuse an unmounted volume at once, and
 *   - does a request for a volume or unit that IS there still allocate?
 *
 * CASES
 *   (1) "UNIT=SYSDA;VOLSER=<vol>", DISP=(NEW,CATLG,DELETE) - a mounted
 *       volume: rc 0, and the catalog has the data set on <vol>.  S99NOMNT
 *       must not cost the ordinary request anything.
 *   (2) "UNIT=SYSDA" alone (ftpd's DEFUNIT shape): rc 0.  S99NOMNT also
 *       means "do not consider offline units", so this is the case that
 *       would show a unit name that only resolves through an offline device.
 *   (3) "UNIT=SYSDA;VOLSER=NOVOL9" - a volume that is not mounted: rc != 0,
 *       AT ONCE.  Before the fix the task waited on IEF238D REPLY DEVICE NAME
 *       OR 'CANCEL' until an operator answered (ftpd#133 through __dsalcf();
 *       JOB01082 for the same flag byte through fopen()).  The probe cannot
 *       see that wait from inside, so READ THE JOB LOG: an IEF238D there
 *       fails this case, whatever the probe printed.  The elapsed seconds
 *       are printed as a second witness.
 *   (4) "UNIT=SYSDA;VOLSER=NOVOL9;MOUNT" - the same volume, but the caller
 *       ASKS for the mount: the operator gets IEF238D, as before the fix.
 *   (5) fopen(..., "wb,...,unit=sysda,volser=novol9,mount") - the same
 *       through fopen()'s create path (#172 set S99NOMNT there too).
 *
 *       (4) and (5) NEED THE OPERATOR.  Each issues a WTO "TSTDSNMT (n)
 *       WAITS" first, then waits on IEF238D REPLY DEVICE NAME OR 'CANCEL'.
 *       Reply R xx,CANCEL - the allocation then fails.  The probe counts a
 *       refusal that took at least 5 seconds as the wait it asked for; one
 *       that came back at once means S99NOMNT was still set.  The job log
 *       shows the IEF238D, which is the evidence.
 *
 * SETUP: none.  The work data set IBMUSER.LIBC370.DSNMT.WORK is created and
 * deleted by (1) and (2); it must NOT exist when the job starts.
 *
 * PARM='<volser>'  a mounted volume reachable through UNIT=SYSDA (default
 * PUB001).
 *
 * BUILD (host), against the branch library - -L build/sdk is load-bearing,
 * see test/mvs/tstfprls.c.  The fix adds no symbol, so there is no symbol to
 * check the module for; the generated src/mvs/dynalloc/@@dsalc.s is the
 * check (OI of X'20' into the RB99 flag byte):
 *
 *     make build
 *     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstdsnmt.c \
 *           -o TSTDSNMT -flinker-output=iebcopy
 *     ld370 --pack TSTDSNMT=TSTDSNMT.iebcopy -o probe -xmit \
 *           --dsn IBMUSER.LIBC370.DSNMTSCR
 *
 * Then upload probe.xmit to IBMUSER.LIBC370.DSNMTXMI, run jcl/recvdsnm.jcl,
 * run jcl/tstdsnmt.jcl.
 *
 * RC: 0 = every check passed, 8 = at least one did not (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "mvs/dynalloc.h" /* __dsalcf(), __dsfree() */
#include "mvs/dscb.h"   /* __locate() */
#include "mvs/wto.h"    /* wtof() */

#define DSN     "IBMUSER.LIBC370.DSNMT.WORK"
#define WAITED  5       /* seconds: a refusal this late was the operator */
#define NEW     "DSN=" DSN ";DISP=(NEW,CATLG,DELETE);DSORG=PS;RECFM=FB;" \
                "LRECL=80;BLKSIZE=800;SPACE=TRK(1,1)"

static int  bad = 0;
static char dd[9];
static char vol[7] = "PUB001";

static void pad44(char *dsn44, const char *dsn)
{
    int i;

    for (i = 0; i < 44 && dsn[i]; i++) dsn44[i] = dsn[i];
    for (; i < 44; i++) dsn44[i] = ' ';
}

/* the volume as the catalog has it; 0 when it could be read */
static int where(char out[7])
{
    LOCWORK lw;
    char    dsn44[44];
    int     rc;

    pad44(dsn44, DSN);
    memset(&lw, 0, sizeof(lw));
    rc = __locate(dsn44, &lw);
    if (rc) { printf("      __locate rc=%d\n", rc); return 1; }
    memcpy(out, lw.volser, 6);
    out[6] = 0;
    return 0;
}

static void drop(void)
{
    if (__dsalcf(dd, "DSN=%s;DISP=(OLD,DELETE)", DSN) == 0) __dsfree(dd);
}

/* ------------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    char   got[7];
    int    rc;
    int    i;
    time_t t0;
    time_t t1;
    FILE   *fp;

    if (argc > 1 && argv[1] && argv[1][0] > ' ') {
        for (i = 0; i < 6 && argv[1][i] > ' '; i++) vol[i] = argv[1][i];
        vol[i] = 0;
    }

    printf("TSTDSNMT - libc370 #181 probe, work dsn '%s', volume %s\n\n",
           DSN, vol);

    /* ---------------------------------------------------------------- */
    printf("(1) \"UNIT=SYSDA;VOLSER=%s\" - a mounted volume\n", vol);
    rc = __dsalcf(dd, NEW ";UNIT=SYSDA;VOLSER=%s", vol);
    printf("    __dsalcf rc=%d dd=%s\n", rc, rc ? "-" : dd);
    if (rc) {
        printf("    *** FAIL - the ordinary request was refused\n");
        bad++;
    }
    else {
        __dsfree(dd);
        if (where(got)) {
            printf("    *** FAIL - the catalog did not answer\n");
            bad++;
        }
        else if (strcmp(got, vol) != 0) {
            printf("    *** FAIL - catalog volser=%s, want %s\n", got, vol);
            bad++;
        }
        else printf("    catalog volser=%s\n    ok\n", got);
        drop();
    }

    /* ---------------------------------------------------------------- */
    printf("\n(2) \"UNIT=SYSDA\" alone\n");
    rc = __dsalcf(dd, NEW ";UNIT=SYSDA");
    printf("    __dsalcf rc=%d dd=%s\n", rc, rc ? "-" : dd);
    if (rc) {
        printf("    *** FAIL - the ordinary request was refused\n");
        bad++;
    }
    else {
        __dsfree(dd);
        if (where(got) == 0) printf("    catalog volser=%s\n", got);
        printf("    ok\n");
        drop();
    }

    /* ---------------------------------------------------------------- */
    printf("\n(3) \"UNIT=SYSDA;VOLSER=NOVOL9\" - a volume that is not there\n");
    t0 = time(NULL);
    rc = __dsalcf(dd, NEW ";UNIT=SYSDA;VOLSER=NOVOL9");
    t1 = time(NULL);
    printf("    __dsalcf rc=%d after %ld s\n", rc, (long)(t1 - t0));
    if (rc == 0) {
        printf("    *** FAIL - it allocated: the volser was ignored\n");
        bad++;
        __dsfree(dd);
        drop();
    }
    else {
        printf("    refused - ok only if the job log shows no IEF238D\n");
    }

    /* ---------------------------------------------------------------- */
    printf("\n(4) \"UNIT=SYSDA;VOLSER=NOVOL9;MOUNT\" - the operator is asked\n");
    wtof("TSTDSNMT (4) WAITS - REPLY CANCEL TO IEF238D");
    t0 = time(NULL);
    rc = __dsalcf(dd, NEW ";UNIT=SYSDA;VOLSER=NOVOL9;MOUNT");
    t1 = time(NULL);
    printf("    __dsalcf rc=%d after %ld s\n", rc, (long)(t1 - t0));
    if (rc == 0) {
        printf("    *** FAIL - it allocated: the volser was ignored\n");
        bad++;
        __dsfree(dd);
        drop();
    }
    else if (t1 - t0 < WAITED) {
        printf("    *** FAIL - refused at once: MOUNT did not reach SVC 99\n");
        bad++;
    }
    else printf("    waited for the operator\n    ok\n");

    /* ---------------------------------------------------------------- */
    printf("\n(5) fopen(\"...,unit=sysda,volser=novol9,mount\") - the same\n");
    wtof("TSTDSNMT (5) WAITS - REPLY CANCEL TO IEF238D");
    t0 = time(NULL);
    fp = fopen("'" DSN "'", "wb,recfm=fb,lrecl=80,blksize=800,"
               "space=trk(1,1),unit=sysda,volser=novol9,mount");
    t1 = time(NULL);
    printf("    fopen %s after %ld s\n", fp ? "opened" : "NULL", (long)(t1 - t0));
    if (fp) {
        printf("    *** FAIL - it opened: the volser was ignored\n");
        bad++;
        fclose(fp);
        drop();
    }
    else if (t1 - t0 < WAITED) {
        printf("    *** FAIL - refused at once: mount did not reach SVC 99\n");
        bad++;
    }
    else printf("    waited for the operator\n    ok\n");

    printf("\nTSTDSNMT %s\n", bad ? "FAILED" : "PASSED");
    return bad ? 8 : 0;
}
