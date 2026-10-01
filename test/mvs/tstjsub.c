/*
 * tstjsub.c - libc370 #79: JESJOB carries the submit time and the input
 * processor's system id (MVS, batch).
 *
 * 1.x's JESJOB had the execution window only (start_time64, end_time64);
 * mvsMF's z/OSMF jobs API has to leave "exec-submitted" empty (mvsmf#208).
 * 2.0 appends submit_time64 (JCTRDRON/JCTRDTON, time and date on the input
 * processor) and sysid (JCTRDSID) at 0x50, behind every 1.x field.
 *
 * The probe lists, through jesjob(), the jobs named in PARM -- by default
 * itself, which is executing (start set, end not), and TSTJHOLD, a job
 * submitted TYPRUN=HOLD and released a while later (jcl/tstjsub.jcl says
 * how) -- and checks for each job:
 *
 *   1. submit_time64 is set, and sysid is not empty
 *   2. submit <= start when the job has started, and for TSTJHOLD
 *      submit < start: a job that waited is the one case where reading
 *      the execution time by mistake (JCTXEQON) would show
 *   3. start  <= end   when it has ended
 *
 * The times print as JES2 recorded them, its local wall clock: make_time()
 * builds the value so that gmtime64() returns that clock unchanged, as for
 * start_time64 since 1.x.  Hold them against the job log ($HASP373).  A 1.x
 * library cannot build this probe: the two fields do not exist there.
 *
 * The names are split by hand: jesjob() calls strtok() itself (to trim
 * blanks), which would end a caller's own strtok() loop after one name.
 *
 * Measured on mvsdev, 2026-10-01 (JOB01066, CC 0): TSTJHOLD (JOB01065), held
 * 20 seconds, submitted 03:06:18 and started 03:06:39, as its $HASP373
 * says; sysid 'DEV1', as $HASP373's "SYS DEV1".
 *
 * PARM='<jobname>[,<jobname>...]'   (default TSTJHOLD,TSTJSUB)
 *
 * BUILD (host):
 *     python3 sdk/mklibc.py build
 *     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstjsub.c \
 *           -flinker-output=iebcopy -o TSTJSUB
 */
#include <stdio.h>
#include <string.h>
#include <mvs/jes2.h>
#include <ext/array.h>
#include <ext/int64.h>
#include <ext/time64.h>

static int fails, checked;

static int is_zero(time64_t *t)
{
    return t->u32[0] == 0 && t->u32[1] == 0;
}

static void show(const char *what, time64_t *t)
{
    struct tm *tm;

    if (is_zero(t)) {
        printf("    %-7s (not set)\n", what);
        return;
    }
    tm = gmtime64(t);
    printf("    %-7s %04d-%02d-%02d %02d:%02d:%02d  (x'%08X%08X')\n", what,
           tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
           tm->tm_hour, tm->tm_min, tm->tm_sec, t->u32[0], t->u32[1]);
}

#define CHECK(cond, msg) \
    do { if (cond) printf("    ok   %s\n", msg); \
         else { fails++; printf("    FAIL %s\n", msg); } } while (0)

static void one(JESJOB *job)
{
    printf("  %-8s %-8s sysid '%s' class %c\n",
           job->jobname, job->jobid, job->sysid, job->eclass);
    show("submit", &job->submit_time64);
    show("start",  &job->start_time64);
    show("end",    &job->end_time64);
    checked++;

    CHECK(!is_zero(&job->submit_time64), "submit time is set");
    CHECK(job->sysid[0] != 0, "sysid is set");
    if (!is_zero(&job->start_time64))
        CHECK(__64_cmp(&job->submit_time64, &job->start_time64) != __64_LARGER,
              "submit <= start");
    if (!is_zero(&job->start_time64) && strcmp((char *) job->jobname, "TSTJHOLD") == 0)
        CHECK(__64_cmp(&job->submit_time64, &job->start_time64) == __64_SMALLER,
              "a held job: submit < start");
    if (!is_zero(&job->end_time64))
        CHECK(__64_cmp(&job->start_time64, &job->end_time64) != __64_LARGER,
              "start <= end");
}

int main(int argc, char **argv)
{
    char        names[120];
    char        *name, *next;
    JES         *jes;
    JESJOB      **jobs;
    unsigned    n, i;

    strcpy(names, argc > 1 && argv[1][0] ? argv[1] : "TSTJHOLD,TSTJSUB");
    printf("TSTJSUB: JESJOB submit_time64 and sysid (#79), jobs %s\n", names);
    printf("  sizeof(JESJOB) = %u (2.0: 96)\n", (unsigned) sizeof(JESJOB));

    jes = jesopen();
    if (!jes) {
        printf("TSTJSUB: jesopen() failed\n");
        return 8;
    }
    for (name = names; name && *name; name = next) {
        next = strchr(name, ',');
        if (next)
            *next++ = 0;
        jobs = jesjob(jes, name, FILTER_JOBNAME, 0);
        n = jobs ? arraycount(&jobs) : 0;
        printf("%s: %u job(s)\n", name, n);
        for (i = 0; i < n; i++)
            one(jobs[i]);
        if (jobs)
            jesjobfr(&jobs);
    }
    jesclose(&jes);

    if (!checked) {
        printf("TSTJSUB: no job found, nothing checked\n");
        return 8;
    }
    printf("TSTJSUB: %d job(s), %s\n", checked, fails ? "FAILED" : "passed");
    return fails ? 8 : 0;
}
