/*
 * tstjesjm.c - libc370 #406 on MVS: jesjob() with a filter longer than
 * its 12-byte buffer.
 *
 * jesjob() copied up to 12 filter characters into filt[12] and stored the
 * NUL behind it.  Now it keeps 11.  The filter "TSTJ" followed by 16
 * asterisks matches this job either way; what the case checks is that
 * the call comes back with this job in the list.  The old version's
 * one-byte overrun lands in jesjob()'s own frame and is not expected to
 * show here: the red step is a control that the probe runs, not a red.
 *
 *   1 jesjob(jes, "TSTJ****************", FILTER_JOBNAME, 0) finds this
 *     job (TSTJESJM)
 *
 * BUILD as test/mvs/tstaradm.c, TSTJESJG / TSTJESJR, IBMUSER.LIBC370.JJSCR;
 * then jcl/recvjesjm.jcl and jcl/tstjesjm.jcl.
 *
 * Measured 2026-10-18 on MVS/CE: both steps find the job, as expected.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <string.h>
#include <mvs/wto.h>
#include <mvs/jes2.h>
#include <ext/array.h>

int main(void)
{
    JES     *jes = jesopen();
    JESJOB  **jobs;
    unsigned n, count;
    int     found = 0;

    if (!jes) {
        wtof("TSTJESJM FAIL: jesopen()");
        return 8;
    }
    jobs = jesjob(jes, "TSTJ****************", FILTER_JOBNAME, 0);
    count = jobs ? arraycount(&jobs) : 0;
    for (n = 0; n < count; n++) {
        wtof("TSTJESJM job %.8s %.8s", jobs[n]->jobname, jobs[n]->jobid);
        if (memcmp(jobs[n]->jobname, "TSTJESJM", 8) == 0) found = 1;
    }
    wtof("TSTJESJM %s: 1 a 20-character filter finds this job",
         found ? "PASS" : "FAIL");
    if (jobs) jesjobfr(&jobs);
    jesclose(&jes);
    return found ? 0 : 8;
}
