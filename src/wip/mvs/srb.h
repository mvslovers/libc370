#ifndef CLIBSRB_H
#define CLIBSRB_H

/* srb.h - storage for an SRB (#248).
**
** Work in progress, never installed: src/wip is not built.  Moves to
** include/mvs/srb.h when something schedules an SRB.
**
** inline_srb_getmain() gets and clears a block of at least SRBSIZE in
** SRB_SUBPOOL (CSA) under MODESET KEY=ZERO; inline_srb_freemain() gives it
** back.  Both need an APF-authorized caller.  Neither schedules anything.
**
** Fixed when it moved here: FREEMAIN read "SP+(%2)", which the macro takes
** without complaint and then loads no subpool at all, freeing from
** subpool 0 what was got from SRB_SUBPOOL.  The out-of-line srb_getmain()
** and srb_freemain() had no implementation and were dropped.
*/
#include <string.h>
#include <ibm/mvs/ihasrb.h>

static __inline void *inline_srb_getmain(unsigned size, int subpool)
{
    void    *storage;

    if (size < SRBSIZE) size    = SRBSIZE;
    if (!subpool)       subpool = SRB_SUBPOOL;

    __asm("MODESET MODE=SUP,KEY=ZERO" : :  : "0", "1", "14", "15");
    __asm("GETMAIN R,LV=(%0),SP=(%1)\n\t"
          "ST\t1,0(,%2)" : :
          "r"(size), "r"(subpool), "r"(&storage) : "0", "1", "14", "15");
          memset(storage, 0, size);
    __asm("MODESET MODE=PROB,KEY=NZERO" : :  : "0", "1", "14", "15");

    return storage;
}


static __inline void inline_srb_freemain(void *storage, unsigned size, int subpool)
{
    if (size < SRBSIZE) size    = SRBSIZE;
    if (!subpool)       subpool = SRB_SUBPOOL;

    __asm("MODESET MODE=SUP,KEY=ZERO" : : : "0", "1", "14", "15");
    __asm("FREEMAIN R,A=(%0),LV=(%1),SP=(%2)\n\t" : :
          "r"(storage), "r"(size), "r"(subpool) : "0", "1", "14", "15");
    __asm("MODESET MODE=PROB,KEY=NZERO" : :  : "0", "1", "14", "15");
}


#endif /* CLIBSRB_H */
