#ifndef MVS_STORAGE_H
#define MVS_STORAGE_H
/* mvs/storage.h - storage: GETMAIN/FREEMAIN and the runtime heap subpool.
**
** libc370 2.0 splits this out of clibos.h (#256).
*/

#include <stddef.h>

/* getmain() returns allocated storage of size in requested subpool or NULL */
/* request with SP > 127 require the caller to be in supervisor state */
void *getmain(unsigned size, unsigned sp);

/* freemain() release storage allocated by getmain() function */
int freemain(void *addr);

/* --- runtime heap subpool (#89) --------------------------------------
   The malloc()/__getm() subpool is a runtime value held in the current
   TCB's own PPA (PPAHEAPS) and recorded per block in the header, so
   free()/__freem() need no variant - a block travels with its subpool.
   Resolution is the current TCB's PPA only; a TCB without a PPA (e.g.
   a cthread task) has its ambient subpool fixed at 0.

   DANGER: the ambient subpool is ambient for EVERYBODY running on this
   TCB, including callbacks that allocate storage meant to outlive the
   current program invocation.  Such storage must be pinned explicitly:
   __getmsp(size, 0) for raw allocations, or a __setsp(0) bracket
   around allocating calls.  Do not set a non-zero ambient subpool
   until every such site in the call graph is pinned (issue #89). */

/* __setsp() set the ambient heap subpool, returns the previous value.
   No-op returning 0 when the current TCB has no PPA.  Usable range in
   problem state is 1-127 (0 is the shared default): subpools above 127
   need supervisor state, so a problem-state caller that sets one gets
   NULL from every allocation until the value is reset - the GETMAIN is
   conditional and fails with a return code, not an abend. */
unsigned char __setsp(unsigned char sp);

/* __getsp() current ambient heap subpool, 0 when the TCB has no PPA */
unsigned char __getsp(void);

/* __getmsp() __getm() with an explicit subpool, ignoring the ambient
   value - how storage that must survive a FREEMAIN SP=n reclaim is
   pinned.  Block is free()/__freem() compatible.  Returns NULL on
   shortage or when the rounded size exceeds 24 bits. */
void *__getmsp(size_t size, unsigned char sp);

/* ---- from 1.x mvssupa.h ------------------------------------------------- */
#pragma linkage(__getm, OS)
void *__getm(size_t sz);
#pragma linkage(__freem, OS)
void __freem(void *ptr);

#endif /* MVS_STORAGE_H */
