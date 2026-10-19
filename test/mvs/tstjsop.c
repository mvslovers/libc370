/*
 * tstjsop.c - libc370 #142 on MVS: jesopen() without a HASPCKPT DD.
 *
 * Up to 2.23.0 jesopen() opened the JES2 checkpoint and spool through the
 * DDs HASPCKPT and HASPACE1 only, so every caller's JCL had to carry them.
 * Now a DD still wins when the step has one; without it the checkpoint
 * SYS1.HASPCKPT is allocated through the catalog, or, when it is not
 * cataloged, on the volume an OBTAIN scan finds it on, and the spool
 * SYS1.HASPACE on the volume the checkpoint names.  jesopen_ex() takes
 * another checkpoint name and uses no DD.
 *
 * PARM selects the case; one module runs in several steps:
 *
 *   NODD   the step has no JES2 DD.  jesopen() opens, jesjob() lists the
 *          jobs (it reads every job's JCT from the spool, so the list
 *          proves both data sets), the DDs are SYSnnnnn and are gone again
 *          after jesclose(); ten open/close rounds, so a DD left behind
 *          would show.  RED (2.23.0): jesopen() returns NULL.
 *          Not under RED: jesopen_ex() with SYS1.HASPCKPT, with a name
 *          that does not exist (NULL, nothing left allocated), and the
 *          OBTAIN scan __jesvol() itself (below).
 *   DD     the step has HASPCKPT and HASPACE1 DDs: they are the ones
 *          opened, they are still allocated after jesclose(), and a second
 *          jesopen() uses them again.  Not under RED: jesopen_ex() with a
 *          name does not use them.
 *
 * THE OBTAIN PATH.  SYS1.HASPCKPT is cataloged on MVS/CE, so jesopen()
 * takes the catalog there and never reaches the scan.  The data sets are
 * not uncataloged to force it - JES2 runs on them.  Instead the probe calls
 * the scan, __jesvol() (src/mvs/jes2/jesvol.c, the function jesopen() calls
 * when the catalog fails), directly:
 *   - for SYS1.HASPCKPT it must name the volume the catalog names
 *     (__locate()); OBTAIN reads the VTOCs and never the catalog, so the two
 *     answers are independent;
 *   - for IBMUSER.JX142.NOCAT, which jcl/tstjsop.jcl creates with
 *     DISP=(NEW,KEEP) and so leaves uncataloged, the catalog allocation must
 *     fail and the scan must find it, and the allocation on that volume
 *     must work: the three calls jesopen() makes, in its order;
 *   - for a name that exists nowhere it must say "not found" after
 *     searching every online volume.
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -I. -L build/sdk \
 *         test/mvs/tstjsop.c -o $S/TSTJSOG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror -DRED test/mvs/tstjsop.c \
 *         -o $S/TSTJSOR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTJSOG=TSTJSOG.iebcopy \
 *         TSTJSOR=TSTJSOR.iebcopy -o jx -xmit --dsn IBMUSER.LIBC370.JXSCR
 *
 * The RED module is built against a library without this change (2.23.0
 * or main before #142: add -I and -L for it), which has no jesopen_ex()
 * and no __jesvol(); -DRED leaves those cases out.
 * Then jcl/recvjsop.jcl and jcl/tstjsop.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <stdlib.h>
#include <string.h>
#include <mvs/wto.h>
#include <mvs/dd.h>
#include <mvs/jes2.h>
#include <mvs/dscb.h>
#include <mvs/dynalloc.h>
#include <ext/array.h>
#ifndef RED
#include "src/mvs/jes2/jesvol.h"
#endif

#define NOCAT   "IBMUSER.JX142.NOCAT"

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTJSOP %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* the number of DDs of the step */
static unsigned ndsab(void)
{
    DSAB        *d;
    unsigned    n   = 0;

    for (d = get_dsab(NULL, NULL); d; d = next_dsab(d, NULL, NULL)) n++;
    return n;
}

/* the DD names of a handle, as strings */
static void ddnames(const JES *jes, char ck[9], char sp[9])
{
    memcpy(ck, jes->cp->ddname, 8);
    ck[8] = 0;
    memcpy(sp, jes->js[0]->ddname, 8);
    sp[8] = 0;
}

/* list the jobs: the count, and the first three */
static unsigned listjobs(JES *jes)
{
    JESJOB      **jobs  = jesjob(jes, NULL, FILTER_NONE, 0);
    unsigned    n       = 0;
    unsigned    i;

    if (jobs) {
        n = arraycount(&jobs);
        for (i = 0; i < n && i < 3; i++)
            wtof("TSTJSOP   job %-8s %-8s", jobs[i]->jobname, jobs[i]->jobid);
        jesjobfr(&jobs);
    }
    wtof("TSTJSOP   %u jobs in the queue", n);
    return n;
}

/* open, list, close; returns 1 when the handle worked */
static int round_trip(JES *jes, int verbose, char ck[9], char sp[9])
{
    unsigned    n;

    if (!jes) return 0;
    ddnames(jes, ck, sp);
    if (verbose) {
        wtof("TSTJSOP   checkpoint DD %s, spool DD %s", ck, sp);
        n = listjobs(jes);
    }
    else {
        JESJOB **jobs = jesjob(jes, NULL, FILTER_NONE, 0);
        n = jobs ? arraycount(&jobs) : 0;
        if (jobs) jesjobfr(&jobs);
    }
    jesclose(&jes);
    return n > 0;
}

static void nodd(void)
{
    char    ck[9]   = "";
    char    sp[9]   = "";
    int     i;
    int     ok      = 1;
    unsigned before = ndsab();
    JES     *jes;

    want(get_dsab(NULL, "HASPCKPT") == NULL, "the step has no HASPCKPT DD");

    jes = jesopen();
    if (!jes) wtof("TSTJSOP   jesopen() returned NULL");
    want(round_trip(jes, 1, ck, sp), "jesopen() without DD lists jobs");
    want(memcmp(ck, "SYS", 3) == 0 && memcmp(sp, "SYS", 3) == 0,
         "both data sets on dynamically allocated DDs");
    want(ck[0] && get_dsab(NULL, ck) == NULL && get_dsab(NULL, sp) == NULL,
         "jesclose() unallocated both DDs");

    for (i = 0; i < 10; i++) ok &= round_trip(jesopen(), 0, ck, sp);
    want(ok, "ten more open/close rounds, each lists jobs");
    wtof("TSTJSOP   DDs in the step: %u before, %u after", before, ndsab());
    want(ndsab() == before, "no DD left behind by the rounds");

#ifndef RED
    jes = jesopen_ex("sys1.haspckpt");
    want(round_trip(jes, 0, ck, sp), "jesopen_ex(\"sys1.haspckpt\")");

    before = ndsab();
    jes = jesopen_ex("NOSUCH.HASPCKPT");
    want(jes == NULL, "jesopen_ex(\"NOSUCH.HASPCKPT\") is NULL");
    want(ndsab() == before, "and leaves no DD allocated");
    if (jes) jesclose(&jes);

    {
        LOCWORK     lw;
        char        vol[7]  = "";
        char        dd[9]   = "";
        unsigned    scanned = 0;
        int         rc;

        memset(&lw, 0, sizeof(lw));
        rc = __locate("SYS1.HASPCKPT", &lw);
        wtof("TSTJSOP   __locate(SYS1.HASPCKPT) rc=%d vol=%-6.6s",
             rc, lw.volser);
        rc = __jesvol("SYS1.HASPCKPT", vol, &scanned);
        wtof("TSTJSOP   __jesvol(SYS1.HASPCKPT) rc=%d vol=%s after %u",
             rc, vol, scanned);
        want(rc == 0 && memcmp(vol, lw.volser, 6) == 0,
             "OBTAIN scan finds SYS1.HASPCKPT where the catalog says");

        rc = __dsalcf(dd, "dsname=%s,disp=shr", NOCAT);
        wtof("TSTJSOP   catalog allocation of " NOCAT " rc=%d", rc);
        want(rc != 0, NOCAT " is not cataloged");
        if (rc == 0) __dsfree(dd);
        rc = __jesvol(NOCAT, vol, &scanned);
        wtof("TSTJSOP   __jesvol(" NOCAT ") rc=%d vol=%s after %u",
             rc, vol, scanned);
        want(rc == 0, "OBTAIN scan finds the uncataloged " NOCAT);
        rc = rc ? -1 : __dsalcf(dd, "dsname=%s,disp=shr,volser=%s",
                                NOCAT, vol);
        want(rc == 0, "and it is allocated on that volume");
        if (rc == 0) __dsfree(dd);

        rc = __jesvol("NOSUCH.HASPCKPT", vol, &scanned);
        wtof("TSTJSOP   __jesvol(NOSUCH.HASPCKPT) rc=%d after %u volumes",
             rc, scanned);
        want(rc == 1 && scanned > 0, "a name on no volume: not found");
    }
#endif
}

static void dd(void)
{
    char    ck[9]   = "";
    char    sp[9]   = "";
    JES     *jes;

    want(get_dsab(NULL, "HASPCKPT") != NULL, "the step has a HASPCKPT DD");

    jes = jesopen();
    want(round_trip(jes, 1, ck, sp), "jesopen() with the DDs lists jobs");
    want(strcmp(ck, "HASPCKPT") == 0 && strcmp(sp, "HASPACE1") == 0,
         "the DDs of the JCL were used");
    wtof("TSTJSOP   after jesclose(): HASPCKPT %s, HASPACE1 %s",
         get_dsab(NULL, "HASPCKPT") ? "allocated" : "gone",
         get_dsab(NULL, "HASPACE1") ? "allocated" : "gone");

    jes = jesopen();
    want(round_trip(jes, 0, ck, sp) && strcmp(ck, "HASPCKPT") == 0,
         "a second jesopen() uses the HASPCKPT DD again");

#ifndef RED
    jes = jesopen_ex("SYS1.HASPCKPT");
    want(round_trip(jes, 0, ck, sp) && memcmp(ck, "SYS", 3) == 0
         && memcmp(sp, "SYS", 3) == 0,
         "jesopen_ex() with a name does not use the DDs");
#endif
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "NODD";

#ifdef RED
    wtof("TSTJSOP RED (library before #142), %s", mode);
#else
    wtof("TSTJSOP GREEN (this tree), %s", mode);
#endif
    if (strcmp(mode, "DD") == 0) dd();
    else nodd();

    wtof("TSTJSOP %s, rc=%d", bad ? "FAILED" : "passed", bad ? 8 : 0);
    return bad ? 8 : 0;
}
