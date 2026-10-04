#include <stdio.h>
#include <stdlib.h>
#include <mvs/env.h>

// The job step supplies, for example:
//
//   //SYSENV   DD *
//   * server settings
//   PORT=8080
//   TRACE = no

int main(void)
{
    const char *trace;
    char       *port;

    if (loadenv("DD:SYSENV") != 0)
        printf("no SYSENV DD, using defaults\n");

    port  = getenv("PORT");
    trace = getenvi("trace");            /* any case matches TRACE */

    printf("port=%s trace=%s\n",
           port  ? port  : "80",
           trace ? trace : "yes");

    setenvi("RETRIES", 3, 0);            /* only if not yet set */
    return 0;
}
