#ifndef CLIBLINK_H
#define CLIBLINK_H
#include <sys/_cc370.h>
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

/* __linkx() - __link() that also reports why LINK failed (#553).
   when LINK takes its error exit (abend S806 for a missing module, S106
   for a fetch error, ...) returns -1 and stores the completion code,
   right-aligned (0x806), at abcode; otherwise *abcode is 0.
   abcode can be NULL.
*/
int __linkx(const char *pgm, void *dcb, void *r1, int *prc,
            unsigned *abcode)                               asm("@@LINKX");

/* __linkdsx() - __linkds() with the completion code of __linkx() (#553) */
int __linkdsx(const char *pgm, void *dcb, void *r1, int *prc,
              unsigned *abcode)                             asm("@@LINKDX");

/* ---- from 1.x clibos.h -------------------------------------------------- */
/* __call() call func with plist value as R1, returns func return code as int */
int __call(void *func, void *plist);

/* clib_find_cde() - find CDE entry for program name */
CDE *clib_find_cde(const char *name)                        asm("@@FNDCDE");

/* __load() - bring load module into storage returning the entry point or 0 for failure */
/* also returns size and access code value if not NULL */
/* dcb value can be NULL to search all possible locations, linklib, tasklib, steplib */
void *__load(void *dcb, const char *module, unsigned *size, char *ac) asm("@@LOAD");

/* __loadx() - LOAD EPLOC= that reports why it failed (#553).
   returns the entry point, or 0 on failure.  When LOAD takes its error
   exit, the completion code (0x806, 0x106, ...) is stored at abcode and
   the reason code at reason; otherwise both are 0.  size, ac, abcode and
   reason can be NULL.  dcb NULL searches as LOAD does.
*/
void *__loadx(void *dcb, const char *module, unsigned *size, char *ac,
              unsigned *abcode, unsigned *reason)           asm("@@LOADX");

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
