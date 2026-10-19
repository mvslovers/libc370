/* __loadx() - LOAD a module and report why LOAD failed */
#include "mvs/link.h"
#include "ctype.h"

/* LOAD EPLOC= with an error exit.  The exit receives the completion code
   right-aligned in R1 (X'00000806') and the reason code in R15 (#553). */
__asm__("\n&FUNC    SETC '__loadx'");
void *__loadx(void *dcb, const char *module, unsigned *size, char *ac,
              unsigned *abcode, unsigned *reason)
{
    void        *epa    = 0;
    unsigned    info    = 0;    /* R1: AC and size, or completion code  */
    unsigned    rsn     = 0;    /* R15 in the error exit                */
    char        ep[12]  = "        ";   /* blank padded EP name         */
    int         n;

    if (!module || *module <= ' ') goto out;

    for (n = 0; n < 8 && module[n]; n++) ep[n] = toupper(module[n]);

    __asm__(
        "LOAD EPLOC=(%3),DCB=(%4),ERRET=LDXERR\n\t"
        "LR\t%0,0            entry point\n\t"
        "LR\t%1,1            AC and size in doublewords\n\t"
        "SR\t%2,%2\n\t"
        "B\tLDXDONE\n"
"LDXERR\tDS\t0H\n\t"
        "SR\t%0,%0           no entry point\n\t"
        "LR\t%1,1            completion code, right-aligned\n\t"
        "LR\t%2,15           reason code\n"
"LDXDONE\tDS\t0H"
        : "=r"(epa), "=r"(info), "=r"(rsn)
        : "r"(ep), "r"(dcb)
        : "0", "1", "14", "15", "memory");

    if (epa) {
        if (size) *size = (info & 0x00FFFFFF) << 3;
        if (ac)   *ac   = (char)(info >> 24);
        info = 0;
    }

out:
    if (abcode) *abcode = info;
    if (reason) *reason = rsn;

    return epa;
}
