#include <stdio.h>
#include <mvs/crt.h>
#include <mvs/wto.h>

/* Called by the start-up before it opens the standard streams
   and before main(). This program writes its report to DD REPORT
   instead of SYSPRINT, and does not start without it. */
int __premain(char *parm, char *pgmname, void **pgmr1)
{
    (void)parm;
    (void)pgmr1;

    stdout = fopen("DD:REPORT", "w");   /* kept by the start-up */
    if (stdout == NULL) {
        /* no stream is open yet: tell the console */
        wtof("%.8s: DD REPORT is missing", pgmname);
        return 16;                      /* main() is not called */
    }
    return 0;                           /* stderr, stdin as usual */
}

int main(void)
{
    puts("REPORT BEGINS");
    return 0;
}
