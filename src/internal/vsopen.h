#ifndef SRC_INTERNAL_VSOPEN_H
#define SRC_INTERNAL_VSOPEN_H
/* src/internal/vsopen.h - internal: the body of __vsopen() with one call
** before OPEN, for __vsopenx() (#623).
**
** __vsopn() builds the handle, its ACB and its RPL as __vsopen() does,
** then calls prep(vs, arg) when prep is not NULL, and then opens the ACB.
** prep may change the ACB and the RPL; a non-zero return ends the open
** with that value as the errno, and the handle is freed.  __vsopen()
** passes NULL, so a program that calls only __vsopen() links none of the
** options.
*/
#include "mvs/vsam.h"

int __vsopn(const char *ddname, VSTYPE t, VSACCESS a, VSMODE m,
            VSFILE **vsfile, int (*prep)(VSFILE *, void *), void *arg);

#endif
