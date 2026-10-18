#include <string.h>
#include <mvs/jes2.h>
#include <mvs/subsys.h>

int jesxdone(SSSO *ssso)
{
    int         rc      = -1;
    SSOB        ssob;

    initssob(&ssob, ssso);          /* initialize the SSOB header   */
    ssob.SSOBFUNC   = SSOBSOUT;     /* request SYSOUT function      */

    ssso->SSSOFLG1  = 0;            /* no request for SYSOUT        */
    ssso->SSSOFLG2  = SSSOCTRL;     /* tell jes we're done          */

    rc = iefssreq(&ssob);           /* send request to jes          */
    if (rc) {
        /* the request did not reach JES: -1 (errno EPERM) for an
           unauthorized caller, else minus IEFSSREQ's return code.
           SSOBRETN stayed 0 then, and that read as success (#406) */
        if (rc > 0) rc = -rc;
        goto quit;
    }
#if 0
    wtodumpf(&ssob, sizeof(ssob), "jesxdone SSOB after IEFSSREQ");
    wtodumpf(ssso, sizeof(SSSO), "jesxdone SSSO after IEFSSREQ");
#endif
    rc = ssob.SSOBRETN;

quit:
    return rc;
}
