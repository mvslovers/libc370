/*
 * tstjsun.c - libc370 #142 on MVS: jesopen() with SYS1.HASPCKPT not
 * cataloged.
 *
 * Without a HASPCKPT DD, jesopen() allocates SYS1.HASPCKPT through the
 * catalog and, when it is not cataloged, on the first online volume an
 * OBTAIN scan (__jesvol()) finds it on.  test/mvs/tstjsop.c could only call
 * the scan directly, because the checkpoint is cataloged on MVS/CE.  This
 * probe lets jesopen() itself take that route: jcl/tstjsun.jcl uncatalogs
 * SYS1.HASPCKPT (IDCAMS DELETE ... NOSCRATCH - the catalog entry only, the
 * data set stays where it is and JES2 keeps running on it) and proves with
 * LISTCAT that it is gone, then runs this probe without any JES2 DD.
 *
 * RUN ONLY IN A THROWAWAY SYSTEM: the job leaves SYS1.HASPCKPT
 * uncataloged.
 *
 * PARM selects the case:
 *
 *   CAT    (before the uncatalog) the baseline: __locate() finds
 *          SYS1.HASPCKPT, jesopen() opens it and finds this job.
 *   UNCAT  (after it) __locate() and a catalog allocation of SYS1.HASPCKPT
 *          fail, so the catalog route is closed; jesopen() must still open
 *          the checkpoint and the spool, jesjob() must find this job in the
 *          queue (it reads the JCT from the spool, so this proves both data
 *          sets), both DDs are dynamically allocated and gone again after
 *          jesclose().  GREEN also shows the volume __jesvol() names.
 *          RED (a library before #142): jesopen() returns NULL.
 *
 * BUILD (from the repository root, after make; S a scratch directory, R a
 * checkout of the commit before #142, built with make):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -I. -L build/sdk \
 *         test/mvs/tstjsun.c -o $S/TSTJSUG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror -DRED -I$R/include -L$R/build/sdk \
 *         test/mvs/tstjsun.c -o $S/TSTJSUR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTJSUG=TSTJSUG.iebcopy \
 *         TSTJSUR=TSTJSUR.iebcopy -o ju -xmit --dsn IBMUSER.LIBC370.JUSCR
 *
 * Then jcl/recvjsun.jcl and jcl/tstjsun.jcl.
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

#define CKPT    "SYS1.HASPCKPT"
#define MYJOB   "TSTJSUN"

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTJSUN %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* is SYS1.HASPCKPT in the catalog?  __locate() and an allocation by name */
static int cataloged(void)
{
    LOCWORK     lw;
    char        dd[9]   = "";
    int         lrc;
    int         arc;

    memset(&lw, 0, sizeof(lw));
    lrc = __locate(CKPT, &lw);
    arc = __dsalcf(dd, "dsname=%s,disp=shr", CKPT);
    if (arc == 0) __dsfree(dd);
    wtof("TSTJSUN   __locate(" CKPT ") rc=%d vol=%-6.6s, "
         "catalog allocation rc=%d", lrc, lw.volser, arc);
    return lrc == 0 && arc == 0;
}

/* open without DD, find this job, close; 1 when all of it worked */
static int open_find_close(void)
{
    JES         *jes;
    JESJOB      **jobs;
    char        ck[9];
    char        sp[9];
    unsigned    n       = 0;
    unsigned    i;

    want(get_dsab(NULL, "HASPCKPT") == NULL
         && get_dsab(NULL, "HASPACE1") == NULL, "the step has no JES2 DD");

    jes = jesopen();
    if (!jes) {
        wtof("TSTJSUN   jesopen() returned NULL");
        return 0;
    }
    memcpy(ck, jes->cp->ddname, 8);
    ck[8] = 0;
    memcpy(sp, jes->js[0]->ddname, 8);
    sp[8] = 0;
    wtof("TSTJSUN   checkpoint DD %s, spool DD %s", ck, sp);
    want(memcmp(ck, "SYS", 3) == 0 && memcmp(sp, "SYS", 3) == 0,
         "both data sets on dynamically allocated DDs");

    jobs = jesjob(jes, MYJOB, FILTER_JOBNAME, 0);
    if (jobs) {
        n = arraycount(&jobs);
        for (i = 0; i < n; i++)
            wtof("TSTJSUN   job %-8s %-8s", jobs[i]->jobname,
                 jobs[i]->jobid);
        jesjobfr(&jobs);
    }
    want(n > 0, "jesjob() finds this job (" MYJOB ") in the queue");

    jesclose(&jes);
    want(get_dsab(NULL, ck) == NULL && get_dsab(NULL, sp) == NULL,
         "jesclose() unallocated both DDs");
    return n > 0;
}

static void cat(void)
{
    want(cataloged(), CKPT " is cataloged (baseline)");
    want(open_find_close(), "jesopen() through the catalog");
}

static void uncat(void)
{
    want(!cataloged(), CKPT " is NOT cataloged, the catalog route is closed");
#ifndef RED
    {
        char        vol[7]  = "";
        unsigned    scanned = 0;
        int         rc      = __jesvol(CKPT, vol, &scanned);

        wtof("TSTJSUN   __jesvol(" CKPT ") rc=%d vol=%s after %u volumes",
             rc, vol, scanned);
    }
#endif
    want(open_find_close(), "jesopen() with the checkpoint uncataloged");
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "UNCAT";

#ifdef RED
    wtof("TSTJSUN RED (library before #142), %s", mode);
#else
    wtof("TSTJSUN GREEN (this tree), %s", mode);
#endif
    if (strcmp(mode, "CAT") == 0) cat();
    else uncat();

    wtof("TSTJSUN %s, rc=%d", bad ? "FAILED" : "passed", bad ? 8 : 0);
    return bad ? 8 : 0;
}
