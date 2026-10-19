/* OSDOPEN.C - open DD for BDAM access (read+write) */
#include "mvs/osio.h"

/* the same OPEN as osxopen(): UPDAT, and the DCB fields completed from the
   JFCB.  The two were copies, and the copy's NULL DCB and unset rc came
   along (#412). */
int
osdopen(DCB *dcb, int typej)
{
    return osxopen(dcb, typej);
}
