/*
 * tstfpunit.c - libc370 #172: UNIT and VOLSER through fopen(), measured on MVS.
 *
 * The host test (test/host/tstfpunit.c) pins that __fpnew() BUILDS DALUNIT and
 * DALVLSER, with the right text, and only when asked.  What it cannot answer:
 *
 *   - does the data set then really land on the volume it was given?
 *
 * So every case creates the data set through fopen() (DISP=NEW), writes one
 * record, closes, and then reads the volume back twice: from the catalog
 * (__locate) and from that volume's VTOC (__dscbdv).  Never from fopen()
 * returning non-NULL - that only says SVC 99 took the request.
 *
 * CASES
 *   (1) control: no unit, no volser.  Reports where the system default puts
 *       the data set.  If that already IS the target volume, (2) and (4)
 *       cannot tell a honoured volser from the default, and the run says
 *       COULD NOT MEASURE instead of passing.  Choose another volume then.
 *   (2) "unit=sysda,volser=<vol>"           -> on <vol>.
 *   (3) "volser=<vol>" without unit=        -> reported, not counted: what
 *       SVC 99 does with a volser and the default unit is what this case is
 *       there to find out.
 *   (4) DATASET_UNIT=SYSDA, DATASET_VOLSER=<vol>, nothing in the mode
 *                                            -> on <vol>.
 *   (5) "unit=sysda,volser=novol9" - a volume that is not mounted
 *                                            -> fopen() returns NULL, AT ONCE.
 *       If it opens, the volser was ignored.  If it waits, the request went
 *       into allocation recovery: the job log shows IEF238D REPLY DEVICE NAME
 *       OR 'CANCEL' and the task stops until an operator answers.  That is
 *       what the first run did (JOB01082, before S99NOMNT), and NULL came only
 *       after a manual R xx,CANCEL.  The probe cannot see that from inside,
 *       so READ THE JOB LOG: an IEF238D there fails this case, whatever the
 *       probe printed.
 *
 * SETUP: none.  The work data set is created and deleted by every case; it
 * must NOT exist when the test starts.
 *
 * PARM='<volser>'  the target volume (default PUB001).  It must be mounted,
 * reachable through UNIT=SYSDA, and NOT where the system default puts a new
 * data set - (1) checks the last.
 *
 * BUILD (host), against the branch library - -L build/sdk is load-bearing,
 * see test/mvs/tstfprls.c.  The pre-fix __fpnew() references neither
 * @@TXUNIT nor @@TXVOLS, so the module must:
 *
 *     make build
 *     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstfpunit.c \
 *           -o TSTFPUNT -flinker-output=iebcopy
 *     python3 -c "print(open('TSTFPUNT','rb').read().count(\
 *                       '@@TXVOLS'.encode('cp037')))"      # must be > 0
 *     ld370 --pack TSTFPUNT=TSTFPUNT.iebcopy -o probe -xmit \
 *           --dsn IBMUSER.LIBC370.UNITSCR
 *
 * Then upload probe.xmit to IBMUSER.MBT.XMIT.IN, run jcl/recvfpun.jcl, run
 * jcl/tstfpunt.jcl.
 *
 * MEASURED 2026-10-01 on mvsdev, PARM='PUB001'.
 *
 *   JOB01082, before S99NOMNT: (1) default WORK00, (2)-(4) on PUB001 by
 *   catalog and VTOC.  (5) did NOT come back: IEF244I UNABLE TO ALLOCATE,
 *   IEF247I (the offline units), IEF238D REPLY DEVICE NAME OR 'CANCEL', and
 *   the job waited 2.5 minutes until R 00,CANCEL - then NULL.
 *
 *   JOB01084, with S99NOMNT: (1) default WORK01, (2)-(4) on PUB001 by catalog
 *   and VTOC, (5) NULL at once - no IEF238D, no IEF244I, step 0.38 s
 *   elapsed.  CC 0000, TSTFPUNT PASSED.
 *
 *   (3) answers the open question: a volser with no unit= is honoured on the
 *   default unit.
 *
 * RC: 0 = every check passed, 8 = at least one did not (it is the COND CODE).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mvs/dynalloc.h" /* __dsalcf(), __dsfree() */
#include "mvs/dscb.h"   /* __locate(), __dscbdv() */

#define DSN     "IBMUSER.TSTFPUNT.WORK"
#define BASE    "wb,recfm=fb,lrecl=80,blksize=800,space=trk(1,1)"

static int  bad = 0;
static int  unmeasured = 0;
static char dd[9];
static char vol[7] = "PUB001";

static void pad44(char *dsn44, const char *dsn)
{
    int i;

    for (i = 0; i < 44 && dsn[i]; i++) dsn44[i] = dsn[i];
    for (; i < 44; i++) dsn44[i] = ' ';
}

/* create through fopen(), one record, close.  0 when it opened. */
static int write_one(const char *mode)
{
    FILE *fp = fopen("'" DSN "'", mode);

    if (!fp) {
        printf("      fopen(\"'%s'\",\"%s\") returned NULL\n", DSN, mode);
        return 1;
    }
    fprintf(fp, "TSTFPUNT - one record\n");
    fclose(fp);
    return 0;
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

/* the format-1 DSCB is in that volume's VTOC */
static int in_vtoc(const char *v)
{
    DSCB d1;
    char dsn44[44];
    int  rc;

    pad44(dsn44, DSN);
    rc = __dscbdv(dsn44, v, &d1);
    if (rc) printf("      __dscbdv(%s) rc=%d\n", v, rc);
    return rc == 0;
}

static void drop(void)
{
    if (__dsalcf(dd, "DSN=%s;DISP=(OLD,DELETE)", DSN) == 0) __dsfree(dd);
}

/* a case that has to land on vol */
static void on_vol(const char *mode, int critical)
{
    char got[7];

    if (write_one(mode)) {
        printf("    OPEN FAILED - nothing allocated, nothing to measure\n");
        if (critical) { printf("    *** FAIL\n"); bad++; }
        else printf("    not counted\n");
        return;
    }
    if (where(got)) {
        printf("    NO MEASURE - the catalog did not answer\n");
        if (critical) unmeasured++;
        drop();
        return;
    }
    printf("    MEASURED catalog volser=%s, want %s\n", got, vol);
    if (strcmp(got, vol) == 0 && in_vtoc(vol)) {
        printf("    in the VTOC of %s: yes\n    ok\n", vol);
    }
    else if (critical) {
        printf("    *** FAIL - not on the volume it was given\n");
        bad++;
    }
    else printf("    not counted\n");
    drop();
}

/* ------------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    char control[7] = "";
    char mode[85];
    int  i;

    if (argc > 1 && argv[1] && argv[1][0] > ' ') {
        for (i = 0; i < 6 && argv[1][i] > ' '; i++) vol[i] = argv[1][i];
        vol[i] = 0;
    }

    printf("TSTFPUNT - libc370 #172 probe, work dsn '%s', target volume %s\n\n",
           DSN, vol);

    /* ---------------------------------------------------------------- */
    printf("(1) control: no unit, no volser - where does the default go?\n");
    if (write_one(BASE) || where(control)) {
        printf("    the control could not be measured; nothing below can be\n"
               "    told apart from the default\n");
        drop();
        printf("\nTSTFPUNT COULD NOT MEASURE\n");
        return 8;
    }
    printf("    MEASURED default volume %s\n", control);
    drop();
    if (strcmp(control, vol) == 0) {
        printf("    that is the target volume itself - (2) and (4) would pass\n"
               "    without the fix.  Run again with PARM='<another volume>'\n");
        printf("\nTSTFPUNT COULD NOT MEASURE\n");
        return 8;
    }

    /* ---------------------------------------------------------------- */
    printf("\n(2) \"unit=sysda,volser=%s\" in the mode string\n", vol);
    sprintf(mode, "%s,unit=sysda,volser=%s", BASE, vol);
    on_vol(mode, 1);

    /* ---------------------------------------------------------------- */
    printf("\n(3) \"volser=%s\" without unit= - reported, not counted\n", vol);
    sprintf(mode, "%s,volser=%s", BASE, vol);
    on_vol(mode, 0);

    /* ---------------------------------------------------------------- */
    printf("\n(4) DATASET_UNIT=SYSDA, DATASET_VOLSER=%s\n", vol);
    setenv("DATASET_UNIT", "SYSDA", 1);
    setenv("DATASET_VOLSER", vol, 1);
    on_vol(BASE, 1);
    unsetenv("DATASET_UNIT");
    unsetenv("DATASET_VOLSER");

    /* ---------------------------------------------------------------- */
    printf("\n(5) \"unit=sysda,volser=novol9\": a volume that is not there\n");
    if (write_one(BASE ",unit=sysda,volser=novol9")) {
        printf("    refused - ok only if the job log shows no IEF238D\n");
    }
    else {
        char got[7] = "?";

        where(got);
        printf("    *** FAIL - it opened, on %s: the volser was ignored\n", got);
        bad++;
        drop();
    }

    printf("\nTSTFPUNT %s\n", bad        ? "FAILED"
                            : unmeasured ? "COULD NOT MEASURE"
                                         : "PASSED");
    return (bad || unmeasured) ? 8 : 0;
}
