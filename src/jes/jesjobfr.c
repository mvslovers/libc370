/* JESJOBFR.C - Free JESJOB array */
#include <stdio.h>
#include <stdlib.h>
#include "ibm/jes2/hct.h"    /* JES Checkpoint Control Table, record 3 in HASPCKPT */
#include "ibm/jes2/jct.h"    /* JES Job Control Table                            */
#include "ibm/jes2/pddb.h"   /* JES PDDB Print Datasets                          */
#include "ibm/jes2/iot.h"    /* JES IOT                                          */
#include "clibjes2.h"   /* JES prototypes */
#include "clibary.h"

int jesjobfr(JESJOB ***pppjesjob)
{
    JESJOB      **jesjob = NULL;
    unsigned    count;
    unsigned    n;

    if (!pppjesjob) goto quit;
    jesjob = *pppjesjob;

    count = arraycount(&jesjob);
    for (n=0; n < count; n++) {
        jesjobf1(&jesjob[n]);
    }
    arrayfree(pppjesjob);
    *pppjesjob = NULL;

quit:
    return 0;
}








