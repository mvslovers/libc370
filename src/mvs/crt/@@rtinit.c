/* @@RTINIT - the runtime part of the C startup (#507).
**
** What @@START does before it parses the parameter and calls main(): open
** the standard streams, load the environment variables from DD SYSENV
** (or ENVIRON), and set the time zone.  @@START calls it at every program
** start; @@CCALL calls it when it builds a C environment for a caller that
** is not C (a COBOL main program), where there is no main() and no
** parameter to parse.
**
** Returns 0, or 1 when a standard stream could not be opened: the cause is
** in the job log, and the streams that were opened are closed again.
*/
#include <mvs/env.h>
#include <time.h>
#include "stdio.h"
#include "stdlib.h"
#include "mvs/wto.h"
#include <errno.h>

/* A standard stream @@start cannot open.  Never a missing DD: "*SYSPRINT"
   and "*SYSTERM" allocate SYSOUT when the DD is not there, and stdin falls
   back to 'NULLFILE'.  So say what failed, with errno, in the job log - it
   used to be "SYSTERM DD not defined" / "SYSIN DD not defined" in a
   dynamic SYSOUT, or nothing at all for stdout, while the cause was
   storage (#254, #277). */
static void startfail(const char *what)
{
    int err = errno;

    wtof("@@START: %s could not be opened: errno %d%s", what, err,
         err == ENOMEM ? ", out of storage - raise REGION" : "");
    errno = err;
}
int
__rtinit(void)
{
    if (!stdout) stdout = fopen("*SYSPRINT", "w");
    if (!stdout) {
        startfail("stdout (SYSPRINT)");
        return 1;
    }

    if (!stderr) stderr = fopen("*SYSTERM", "w");
    if (!stderr) {
        startfail("stderr (SYSTERM)");
        printf("stderr could not be opened, errno %d\n", errno);
        fclose(stdout);
        return 1;
    }

    if (!stdin) stdin = fopen("dd:SYSIN", "r");
    if (!stdin) stdin = fopen("'NULLFILE'", "r");
    if (!stdin) {
        startfail("stdin (SYSIN, then NULLFILE)");
        fprintf(stderr, "stdin could not be opened, errno %d\n", errno);
        fclose(stdout);
        fclose(stderr);
        return 1;
    }

    /* load any environment variables */
    if (loadenv("dd:SYSENV")) {
        /* no SYSENV DD, try ENVIRON DD */
        loadenv("dd:ENVIRON");
    }

    /* initialize time zone offset for this thread */
    tzset();

    return 0;
}
