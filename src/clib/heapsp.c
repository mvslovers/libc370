#include <clib.h>
#include <clibos.h>
#include <clibppa.h>
#include <clibstr.h>
#include <string.h>

/* Runtime heap subpool (#89).
 *
 * The ambient value lives in PPAHEAPS (+0x22) of the current TCB's own
 * PPA and is what @@GETM records in the high byte of the header word,
 * where @@FREEM reads it back - so free() needs no variant and blocks
 * travel with their subpool.
 *
 * Resolution here is deliberately TIER-1 ONLY, the same walk @@GETM
 * inlines: PSATOLD -> TCB -> TCBFSAB -> 8(fsa), validated (nonzero,
 * 24-bit, PPA eyecatcher).  No __ppaget() fallback tiers: on a cthread
 * TCB (which has no PPA) the owner-TCB fallback would let __setsp()
 * mutate the MAIN task's ambient subpool while @@GETM on this TCB keeps
 * allocating from subpool 0 - a lie with an address-space-wide blast
 * radius.  No PPA means the ambient subpool is fixed at 0 here, full
 * stop. */

static CLIBPPA *heapppa(void)
{
    unsigned    *psa = 0;                       /* low core == PSA      */
    unsigned    tcb  = psa[0x21C / 4];          /* PSATOLD              */
    unsigned    fsa;
    unsigned    ppa;

    if (!tcb) return (CLIBPPA *)0;
    fsa = *(unsigned *)(tcb + 0x70) & 0x00FFFFFF;   /* TCBFSAB (AL3)    */
    if (!fsa) return (CLIBPPA *)0;
    ppa = *(unsigned *)(fsa + 8);               /* "next" save area     */
    if (!ppa || ppa > 0x00FFFFFF) return (CLIBPPA *)0;
    if (memcmp(((CLIBPPA *)ppa)->ppaeye, PPAEYE, 4) != 0) return (CLIBPPA *)0;
    return (CLIBPPA *)ppa;
}

__asm__("\n&FUNC    SETC '__setsp'");
unsigned char __setsp(unsigned char sp)
{
    CLIBPPA         *ppa = heapppa();
    unsigned char   old;

    if (!ppa) return 0;                 /* no PPA, ambient is fixed at 0 */
    old = (unsigned char)ppa->ppaheaps;
    ppa->ppaheaps = (char)sp;
    return old;
}

__asm__("\n&FUNC    SETC '__getsp'");
unsigned char __getsp(void)
{
    CLIBPPA     *ppa = heapppa();

    return ppa ? (unsigned char)ppa->ppaheaps : 0;
}

__asm__("\n&FUNC    SETC '__getmsp'");
void *__getmsp(size_t size, unsigned char sp)
{
    int         rc  = 0;
    void        *r1 = (void *)0;
    unsigned    lv;
    unsigned    usp = sp;

    if (!size || size > 0x00FFFFFF) return (void *)0;

    /* same prefix and rounding as @@GETM: 8 byte header, 64 byte
       multiple, and the rounded value must stay clear of the header
       word's subpool byte */
    lv = ((unsigned)size + 8 + (64 - 1)) & 0xFFFFFFC0;
    if (lv > 0x00FFFFFF) return (void *)0;

    __asm__("GETMAIN RC,LV=(%2),SP=(%3)\n\t"
            "LR\t%0,15              save the return code\n\t"
            "LR\t%1,1               save the returned address"
            : "=r"(rc), "=r"(r1)
            : "r"(lv), "r"(usp)
            : "0", "1", "14", "15");
    if (rc) return (void *)0;

    /* @@GETM's header, not getmain()'s: +4 is the plain requested size
       because realloc() reads it */
    *(unsigned *)(r1 + 0) = (usp << 24) | lv;   /* SP||LV for @@FREEM   */
    *(unsigned *)(r1 + 4) = (unsigned)size;     /* caller requested size */
    return r1 + 8;
}
