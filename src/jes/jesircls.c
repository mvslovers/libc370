
#include <stdio.h>
#include <mvssupa.h>
#include "clibvsam.h"
#include "clibjes2.h"

/* Close the internal reader, jobid feedback discarded.  Callers that need
 * the jobid must use jesircl2() -- reading rpl.rplrbar through the handle
 * after this returns is a use-after-free (#118).  The ENDREQ, the RPL
 * work-area release (#115) and the close itself all live in jesircl2().
 */
int jesircls(VSFILE *vsfile)
{
    return jesircl2(vsfile, NULL);
}
