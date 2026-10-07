#include <stdio.h>
#include <mvs/recovery.h>
#include <mvs/wto.h>

struct job {
    const char *name;
    int        *result;
};

/* Stands for work that may abend: it stores through
   a pointer it was given. */
static void work(struct job *job, int value)
{
    *job->result = value;
}

static void report(const char *name, int rc)
{
    if (rc < 0)
        printf("%s: no recovery routine, rc %d\n", name, rc);
    else if (rc == 0)
        printf("%s: completed\n", name);
    else if ((rc >> 12) & 0xFFF)
        printf("%s: abend S%03X\n", name, (rc >> 12) & 0xFFF);
    else
        printf("%s: abend U%04d\n", name, rc & 0xFFF);
}

int main(void)
{
    int        value = 0;
    struct job good  = { "GOOD", &value };
    struct job bad   = { "BAD", NULL };     /* stores into low storage */
    int        rc;
    int        worst = 0;

    wtof("GUARD01I GUARD STARTED");

    rc = try(work, &good, 42);              /* results come back      */
    report(good.name, rc);                  /* through the arguments  */
    if (rc != 0)
        worst = 8;

    rc = try(work, &bad, 42);
    report(bad.name, rc);
    if (rc != 0)
        worst = 8;

    printf("value=%d\n", value);
    return worst;
}
