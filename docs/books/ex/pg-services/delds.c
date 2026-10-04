#include <stdio.h>
#include <mvs/idcams.h>

struct result {
    int first;                      /* first message that matters */
};

static void collect(void *arg, int msgno, const char *text, int len)
{
    struct result *r = arg;

    (void) text;
    (void) len;
    if (!r->first && msgno && msgno != 1 && msgno != 2)
        r->first = msgno;
}

/* Delete a data set that may not exist. 0: deleted or not there. */
int delete_if_exists(const char *dsn)
{
    struct result r = { 0 };
    int           cc;

    cc = idcams_sysprint(collect, &r, " DELETE '%s'", dsn);
    if (cc == 0)
        return 0;
    if (cc == 8 && r.first == 3012)     /* IDC3012I: entry not found */
        return 0;
    if (cc < 0)
        printf("IDCAMS could not be called, rc=%d\n", cc);
    else
        printf("DELETE %s: condition code %d, message IDC%04dI\n",
               dsn, cc, r.first);
    return cc < 0 ? 16 : cc;
}

int main(int argc, char **argv)
{
    return argc > 1 ? delete_if_exists(argv[1]) : 8;
}
