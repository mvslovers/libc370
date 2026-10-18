#include <ext/strutil.h>
#include <ibm/mvs/iefsscs.h>
#include <ibm/mvs/iefssso.h>
#include <string.h>
#include <mvs/jes2.h>
#include <mvs/subsys.h>

int jescanj(const char *jobname, const char *jobid, int purge)
{
    int         rc      = -1;
    SSOB        ssob;
    SSCS        sscs;

    initssob(&ssob, &sscs);         /* initialize the SSOB header   */
    ssob.SSOBFUNC   = SSOBCANC;     /* request CACNEL function      */

    memset(&sscs, 0, sizeof(SSCS)); /* clear the CANCEL parm list   */
    sscs.SSCSLEN    = SSCSIZE;      /* length of CANCEL parm list   */

    if (jobname) {
        strcpyp(sscs.SSCSJOBN, sizeof(sscs.SSCSJOBN), (void*)jobname, ' ');
    }

    if (jobid) {
        strcpyp(sscs.SSCSJOBI, sizeof(sscs.SSCSJOBI), (void*)jobid, ' ');
    }

    if (purge) {
        sscs.SSCSFLGS = SSCSCOUT;   /* purge the output             */
    }

    sscs.SSCSDIMP = SSCSELSZ;       /* size of SSCSARAY area        */

    rc = iefssreq(&ssob);           /* send request to jes          */
    if (rc) {
        /* the request did not reach JES: -1 (errno EPERM) for an
           unauthorized caller, else minus IEFSSREQ's return code.
           SSOBRETN stayed 0 then, and that read as success (#406) */
        if (rc > 0) rc = -rc;
        goto quit;
    }

    rc = ssob.SSOBRETN;

quit:
    return rc;
}
