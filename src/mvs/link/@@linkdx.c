/* __linkdsx() - __linkds() with the completion code of a failed LINK */
#include "mvs/link.h"
#include "mvs/recovery.h"

/* runs under try(): ESTAE abend report without dump, then __linkx() */
__asm__("\n&FUNC    SETC 'linkdx'");
static int linkdx(const char *pgm, void *dcb, void *r1, int *prc,
                  unsigned *abcode)
{
    int         rc;

    abendrpt(ESTAE_CREATE, DUMP_DEFAULT | ABRPT_QUIET_TERM);
    rc = __linkx(pgm, dcb, r1, prc, abcode);
    abendrpt(ESTAE_DELETE, DUMP_DEFAULT);

    return rc;
}

/* 0, the abend code (X'00sssuuu') or a negative value when the ESTAE
   could not be created; *abcode is nonzero only when LINK took its
   error exit, which is no abend (#553) */
__asm__("\n&FUNC    SETC '__linkdsx'");
int __linkdsx(const char *pgm, void *dcb, void *r1, int *prc,
              unsigned *abcode)
{
    if (abcode) *abcode = 0;

    return try(linkdx, pgm, dcb, r1, prc, abcode);
}
