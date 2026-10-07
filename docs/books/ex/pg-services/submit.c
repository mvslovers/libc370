#include <stdio.h>
#include <string.h>
#include <mvs/ecb.h>
#include <mvs/jes2.h>
#include <ext/array.h>

static char *jcl[] = {
    "//DEMOJOB  JOB (1),'DEMO',CLASS=A,MSGCLASS=A",
    "//STEP1    EXEC PGM=IEFBR14",
    NULL
};

/* Submit a job; return 0 and its job ID, e.g. "JOB01234". */
static int submit(char jobid[9])
{
    VSFILE        *rdr;
    unsigned char id[8];
    int           i, rc;

    rc = jesiropn(&rdr);
    if (rc) return rc;
    for (i = 0; jcl[i]; i++)
        if (jesirput(rdr, jcl[i]) != 0) break;
    rc = jesircl2(rdr, id);             /* also closes the reader */
    memcpy(jobid, id, 8);
    jobid[8] = 0;                       /* the ID is not terminated */
    return rc;
}

/* Look at the job once: 1 ended, 0 not yet, -1 not found. */
static int ended(const char *jobid, unsigned *completion)
{
    JES     *jes = jesopen();           /* a fresh copy each time */
    JESJOB  **jobs;
    int     rc = -1;

    if (!jes) return -1;
    jobs = jesjob(jes, jobid, FILTER_JOBID, 0);
    if (jobs && arraycount(&jobs) > 0) {
        JESJOB *job = jobs[0];

        rc = job->q_type == _OUTPUT || job->q_type == _HARDCPY;
        *completion = job->completion;
    }
    jesjobfr(&jobs);
    jesclose(&jes);
    return rc;
}

int main(void)
{
    char     jobid[9];
    unsigned completion = 0;
    int      i, rc = 0;

    if (submit(jobid)) {
        printf("submit failed\n");
        return 8;
    }
    printf("submitted %s\n", jobid);

    for (i = 0; i < 60; i++) {          /* at most two minutes */
        ECB pause = 0;

        rc = ended(jobid, &completion);
        if (rc != 0) break;
        if (ecb_timed_wait(&pause, 200, 1) < 0) break;
    }

    if (rc == 1 && (completion >> 24) == 0x77)
        printf("%s ended, abend %03X, cc %u\n", jobid,
               (completion >> 12) & 0xFFF, completion & 0xFFF);
    else
        printf("%s: state %d, completion %08X\n", jobid, rc, completion);
    return rc == 1 ? 0 : 4;
}
