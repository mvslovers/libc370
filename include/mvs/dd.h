#ifndef MVS_DD_H
#define MVS_DD_H
/* mvs/dd.h - DD lookup: DSAB and TIOT chains.
**
** libc370 2.0 merges clibdsab.h and clibtiot.h into this header (#256).
** Their declarations are unchanged and appear in dependency order.
*/

/* ---- 1.x clibdsab.h -------------------------------------------------- */
#include <ibm/mvs/ihadsab.h>

/* get_dsab(tcbptr, ddname) - Get DSAB pointer, NULL if not found.
 * tcbptr is TCB address or NULL to use this task TCB.
 * (ddname==NULL) : returns first DSAB ptr for Job Step
 * (ddname > " ") : returns DSAB ptr that matches ddname.
*/
DSAB *get_dsab(void *tcbptr, const char *ddname)                        asm("@@GTDSAB");

/* next_dsab(dsab,ddname) - Get next DSAB for this DSAB and ddname */
DSAB *next_dsab(DSAB *dsab, void *tcbptr, const char *ddname)           asm("@@NXDSAB");

/* ---- 1.x clibtiot.h -------------------------------------------------- */
#include "ibm/mvs/ieftiot1.h"

/* __tiot() - retieve TIOT address */
TIOT *__tiot(void)										asm("@@TIOT");

/* __jobname() - retrieve address of 8 character job name from TIOT */
const char *__jobname(void)								asm("@@JOBNAM");

#endif /* MVS_DD_H */
