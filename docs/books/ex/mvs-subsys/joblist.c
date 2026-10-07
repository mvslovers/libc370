#include <stdio.h>
#include <string.h>
#include <mvs/jes2.h>
#include <ext/array.h>

static int show(const char *line, unsigned len, void *arg)
{
    printf("  %.*s\n", (int)len, line);
    return 0;                           /* continue */
}

int main(int argc, char **argv)
{
    const char *filter = argc > 1 ? argv[1] : "*";
    JES      *jes;
    JESJOB  **jobs;
    JESPRST   st;
    unsigned  i, n;

    if (strlen(filter) > 11) {          /* jesjob() takes at most 11 */
        fprintf(stderr, "filter too long: %s\n", filter);
        return 8;
    }
    jes = jesopen();
    if (!jes) return 8;

    jobs = jesjob(jes, filter, FILTER_JOBNAME, 1);
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
