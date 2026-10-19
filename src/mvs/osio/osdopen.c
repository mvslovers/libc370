/* OSDOPEN.C - open DD for BDAM access */
#include <stddef.h>
#include "mvs/osio.h"

/* the same OPEN as osxopen(): UPDAT, and the DCB fields completed from the
   JFCB.  The two were copies, and the copy's NULL DCB and unset rc came
   along (#412). */
int
osdopen(DCB *dcb, int typej)
{
    return osxopenm(dcb, typej, NULL);
}

/* the same OPEN as osxopenm(): mode "r" opens for INPUT (#412) */
int
osdopenm(DCB *dcb, int typej, const char *mode)
{
    return osxopenm(dcb, typej, mode);
}
