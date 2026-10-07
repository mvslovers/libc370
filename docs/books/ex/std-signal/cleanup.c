#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

static FILE *report;

static void on_abort(int sig)
{
    (void)sig;
    fputs("*** run ended by abort ***\n", report);
}

static long add_up(void)
{
    return -1;                          /* stands for real work */
}

int main(void)
{
    long total;

    report = fopen("dd:REPORT", "w");
    if (report == NULL)
        return 8;
    signal(SIGABRT, on_abort);

    total = add_up();
    if (total < 0)
        abort();        /* on_abort runs, then exit(EXIT_FAILURE) */

    fprintf(report, "total %ld\n", total);
    fclose(report);
    return 0;
}
