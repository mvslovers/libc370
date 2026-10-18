#include <ibm/mvs/iefssobh.h>
#include <mvs/apf.h>
#include <errno.h>

int iefssreq(SSOB *ssob)
{
    int         rc      = 0;
    unsigned    *psa    = (unsigned *)0;
    unsigned    *cvt    = (unsigned *)psa[16/4];
    unsigned    *jesct  = (unsigned *)cvt[296/4];
    int         (*ssreq)(SSOB *) = (void*)jesct[20/4];
    int         sup     = __issup();

    /* MODESET is SVC 107: an unauthorized caller got S047 instead of an
       error, and a caller already in supervisor state was returned in
       problem state (#402) */
    if (!sup && !__isauth()) {
        errno = EPERM;
        return -1;
    }

    /* we need to set the high order bit in the SSOB pointer */
    ssob = (SSOB*) ((unsigned)ssob + 0x80000000);

    if (!sup) __asm("MODESET MODE=SUP" : : : "15","0","1", "memory");

    rc = ssreq(ssob);

    if (!sup) __asm("MODESET MODE=PROB" : : : "15","0","1", "memory");

    return rc;
}
