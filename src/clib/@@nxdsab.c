#include <stdio.h>
#include <string.h>
#include <mvs/crt.h>
#include <mvs/wto.h>
#include <stddef.h>
#include <ibm/mvs/ihacde.h>
#include <mvs/dd.h>
#include <ibm/mvs/iezjscb.h>
#include <ibm/mvs/ieftiot1.h>
#include <ibm/mvs/iefjfcbn.h>

DSAB *next_dsab(DSAB *dsab, void *tcbptr, const char *ddname)
{
    TIOTDD  *tiotdd;

    if (!dsab) {
        /* get the first dsab for this ddname */
        dsab = get_dsab(tcbptr, ddname);
        goto quit;
    }

    /* just to make sure we don't go completely off the rails */
    if (memcmp(dsab->dsabid, "DSAB", 4) != 0) {
        /* bad eye catcher, return NULL */
        dsab = NULL;
        goto quit;
    }

    /* point to next dsab */
    dsab = dsab->dsabfchn;

    if (!ddname) goto quit;
    if (ddname[0]==' ' || ddname==0) goto quit;
    
    if (dsab) {
        /* just to make sure we don't go completely off the rails */
        if (memcmp(dsab->dsabid, "DSAB", 4) != 0) {
            /* bad eye catcher, return NULL */
            dsab = NULL;
            goto quit;
        }

        tiotdd = dsab->dsabtiot;

        /* any next for a ddname must be spaces for the ddanme */
        if (memcmp(tiotdd->TIOEDDNM, "        ", 8)==0) {
            goto quit;
        }
    }

    dsab = NULL;

quit:
    return dsab;
}
