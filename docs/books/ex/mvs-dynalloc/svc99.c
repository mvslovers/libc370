#include <stdio.h>
#include <mvs/dynalloc.h>
#include <ext/array.h>

/* Allocate SYS1.MACLIB to DD MACLIB with SVC 99 directly, so that
   the error and information codes of the request block can be shown. */
int main(void)
{
    TXT99       **tu = NULL;
    RB99        rb   = {0};
    unsigned    n;
    int         rc   = 12;

    if (__txddn(&tu, "MACLIB") ||
        __txdsn(&tu, "SYS1.MACLIB") ||
        __txshr(&tu, NULL))
        goto quit;

    n = arraycount(&tu);                /* mark the last text unit */
    tu[n - 1] = (TXT99 *)((unsigned)tu[n - 1] | 0x80000000);

    rb.len    = sizeof(RB99);
    rb.request = S99VRBAL;
    rb.txtptr = tu;

    rc = __svc99(&rb);
    if (rc)
        printf("SVC 99 rc=%d error=%04X info=%04X\n",
               rc, (unsigned short)rb.error, (unsigned short)rb.info);

quit:
    FreeTXT99Array(&tu);
    return rc;
}
