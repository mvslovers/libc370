#ifndef SRC_INTERNAL_PDSUPD_H
#define SRC_INTERNAL_PDSUPD_H
/* src/internal/pdsupd.h - internal: one STOW on a PDS that the call
** allocates and opens itself (#143).  See src/mvs/pds/@@pdsupd.c. */

/* allocate dsn DISP=SHR - on volume vol when vol is not NULL, through the
   catalog when it is - open it for output, STOW area with func ('D', 'C'),
   close and free it.  The STOW return code, or -2 (not allocated, or not
   on vol) or -3 (not opened); -1 for out of memory. */
extern int __pdsupd(const char *dsn, const char *vol, void *area, int func);

/* copy member name mem into an 8-byte field, upper case, blank padded */
extern void __pdsmem(char *field, const char *mem);

#endif /* SRC_INTERNAL_PDSUPD_H */
