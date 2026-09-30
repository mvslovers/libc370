#ifndef CLIBLINK_H
#define CLIBLINK_H
#include <ibm/mvs/ihacde.h>

/* link to external program, dcb, r1 and prc can be NULL
   returns -1 on non-ABEND link failure, otherwise pgm return code.
   if prc is not NULL then return code is stored at prc.
   note: not estae protected.
*/
int __link(const char *pgm, void *dcb, void *r1, int *prc);

/* link to external program, dcb, rc and prc can be NULL
   returns 0==success otherwise ABEND code.
   if prc is not NULL then pgm or __link() return code is stored at prc.
   note: uses try() to handle ABEND, no dump on failures.
*/
int __linkt(const char *pgm, void *dcb, void *r1, int *prc);

/* link to external program, dcb, rc and prc can be NULL
   returns 0==success otherwise ABEND code.
   if prc is not NULL then pgm or __link() return code is stored at prc.
   note: uses try() to handle ABEND, traceback stack, no dump on failures.
*/
int __linkds(const char *pgm, void *dcb, void *r1, int *prc);

/* ---- from 1.x clibos.h -------------------------------------------------- */
/* __call() call func with plist value as R1, returns func return code as int */
int __call(void *func, void *plist);

/* clib_find_cde() - find CDE entry for program name */
CDE *clib_find_cde(const char *name)                        asm("@@FNDCDE");

/* __load() - bring load module into storage returning the entry point or 0 for failure */
/* also returns size and access code value if not NULL */
/* dcb value can be NULL to search all possible locations, linklib, tasklib, steplib */
void *__load(void *dcb, const char *module, unsigned *size, char *ac) asm("@@LOAD");

/* __delete() - remove module from storage, return 0 on success, 4 if not found */
int __delete(const char *module);

/* __loadhi() - load module into high memory (CSA subpool 241) */
/* returns 0 on success, 4 if failure (wto messages may be issued) */
/* notes:
**  The load module is loaded and read from the STEPLIB dataset only.
**  The caller must be APF authorized and key zero to use __loadhi().
**  If authorized but not in key zero use:
**      super_key_do(PSWKEY0, __loadhi, module, &lpa, &epa, &size).
*/
int __loadhi(const char *module, void **lpa, void **epa, unsigned *size) asm("@@LOADHI");

#endif // CLIBLINK_H
