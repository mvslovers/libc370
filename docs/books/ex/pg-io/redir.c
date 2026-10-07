#include <errno.h>
#include <stdio.h>

/* Send stdout to DD REPORT for the rest of the run. */
int main(void)
{
    puts("this line goes to SYSPRINT");

    errno = 0;
    if (freopen("DD:REPORT", "w", stdout) == NULL) {
        fputs("REDIR: cannot open DD REPORT\n", stderr);
        return 12;
    }
    if (errno == ENOSPC || errno == EIO)
        fputs("REDIR: the end of SYSPRINT was lost\n", stderr);

    puts("this line goes to REPORT");
    return 0;
}
