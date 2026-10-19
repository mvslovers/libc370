/* __linkx() - LINK to an external program and report why LINK failed */
#include "mvs/link.h"
#include "ctype.h"

/* LINK's error exit receives the completion code right-aligned in R1
   (X'00000806' for a missing module); R15 holds no reason code there,
   so only the completion code is passed on (#553). */
__asm__("\n&FUNC    SETC '__linkx'");
int __linkx(const char *pgm, void *dcb, void *r1, int *prc, unsigned *abcode)
{
    int         rc      = -1;   /* -1 until LINK returns                */
    unsigned    code    = 0;    /* R1 in LINK's error exit              */
    char        ep[12]  = "        ";   /* blank padded EP name         */
    unsigned    svc[3];         /* LINK parameter list, extended form   */
    int         n;

    if (!pgm || !*pgm) goto out;

    for (n = 0; n < 8 && pgm[n]; n++) ep[n] = toupper(pgm[n]);

    svc[0] = (unsigned)ep;
    svc[1] = (unsigned)dcb | 0x80000000U;   /* error exit word follows */

    __asm(
        "LA\t0,LXERR         => error exit\n\t"
        "ST\t0,8(0,%2)       third word of the LINK list\n\t"
        "LR\t1,%3            => parameter list of the program\n\t"
        "LR\t15,%2           => LINK parameter list\n\t"
        "SVC\t6              LINK\n\t"
        "SR\t%1,%1           returned: no completion code\n\t"
        "B\tLXDONE\n"
"LXERR\tDS\t0H\n\t"
        "LR\t%1,1            completion code, right-aligned\n\t"
        "L\t15,=F'-1'        LINK failed\n"
"LXDONE\tDS\t0H\n\t"
        "LR\t%0,15"
        : "=r"(rc), "=r"(code)
        : "r"(svc), "r"(r1)
        : "0", "1", "14", "15", "memory");

out:
    if (prc)    *prc    = rc;
    if (abcode) *abcode = code;

    return rc;
}
