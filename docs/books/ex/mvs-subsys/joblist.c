#include <stdio.h>
#include <mvs/jes2.h>
#include <ext/array.h>

static int show(const char *line, unsigned len, void *arg)
{
    printf("  %.*s\n", (int)len, line);
    return 0;                           /* continue */
}

int main(int argc, char **argv)
{
    JES      *jes  = jesopen();
    JESJOB  **jobs;
    JESPRST   st;
    unsigned  i, n;

    if (!jes) return 8;

    jobs = jesjob(jes, argc > 1 ? argv[1] : "*",
                  FILTER_JOBNAME, 1);
    n = arraycount(&jobs);
    for (i = 0; i < n; i++) {
        JESJOB *job = jobs[i];

        printf("%-8s %-8s owner %s\n",
               job->jobname, job->jobid, job->owner);
        if (jesprint(jes, job, DSID_OUHJL, show, NULL, &st) == 0
            && st.reason != JESPR_END && st.reason != JESPR_OPENEND)
            printf("  (log incomplete, reason %d)\n", st.reason);
    }

    jesjobfr(&jobs);
    jesclose(&jes);
    return 0;
}
