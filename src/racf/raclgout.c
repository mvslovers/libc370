/* RACLGOUT.C - racf_logout()
** releases ACEE.
*/
#include "racf.h"
#include "clibos.h"                 /* __cas() */

__asm__("\n&FUNC    SETC 'racf_logout'");
int
racf_logout(ACEE **acee)
{
    volatile int    rc          = 0;
    int             sup         = 0;
    int             key         = 0;
    unsigned        *psa        = (unsigned *)0;
    unsigned        *ascb       = (unsigned *)psa[0x224/4]; /* A(ASCB)      */
    unsigned        *asxb       = (unsigned *)ascb[0x6C/4]; /* A(ASXB)      */
    ACEE            **asxbsenv  = (ACEE **)  &asxb[0xC8/4]; /* A(ASXBSENV)  */
    ACEE            *delacee    = acee ? *acee : (ACEE *)0;
    ACEE            *expect;
    RACINIT         plist;

    __asm__("XC\t0(0,%0),0(%0)      clear plist *** executed ***\n\t"
            "EX\t%1,*-6" : : "r"(&plist), "r"(sizeof(plist)-1));
    plist.len   = sizeof(plist);

    __asm__("\n"
"*\n"
"* See if we're in supervisor state\n"
"*\n"
"         TESTAUTH FCTN=0,STATE=YES,KEY=NO,RBLEVEL=1\n\tST\t15,%0" : "=m"(rc)
        : : "1", "14", "15");
    if (rc==0) {
        /* we're in supervisor state */
        sup = 1;
    }

    if (sup) {
        /* we're in supervisor state, switch to key 0 */
        __asm__("\n"
"*\n"
"* we're in supervisor state, switch to key 0\n"
"*\n"
"         IPK\t,\n\tST\t2,%0\n\tSPKA\t0(0)" : "=m"(key) : : "2");
    }
    else {
        __asm__("\n"
"*\n"
"* enter supervisor state\n"
"*\n"
"         MODESET KEY=ZERO,MODE=SUP\n" : : : "1", "14", "15");
    }

    /* The ACEE to delete travels in the parameter list (ACEE= below, offset
    ** X'34'), which is where RACINIT looks for it -- ASXBSENV is only its
    ** fallback when that field is zero.  So it does not have to be parked
    ** there first, and it must not be: in a server with one TCB per user the
    ** value being displaced is routinely ANOTHER session's ACEE, and putting
    ** it back afterwards re-pins an identity its owner has already moved on
    ** from (#64, mvslovers/ftpd#64).
    */
    __asm__("\n"
"*\n"
"* delete ACEE\n"
"*\n"
"         RACINIT ENVIR=DELETE,ACEE=(%1),MF=(E,%2)\n"
"         ST\t15,%0" : "=m"(rc) : "r"(acee), "m"(plist) : "1", "14", "15");

    /* One thing does have to be done by hand.  If ASXBSENV points at the ACEE
    ** just deleted it must be cleared, and RAKF does NOT do it -- measured,
    ** test/mvs/tstracfl.c case (4): after RACINIT ENVIR=DELETE the field was
    ** still the dead pointer.  An address space resting on freed storage is
    ** worse than one resting on none, because the next authorization decision
    ** follows that pointer.
    **
    ** __cas() rather than a compare and a store, so a concurrent writer on
    ** another TCB is never clobbered: the zero goes in only if the field is
    ** still the dead pointer, which is exactly the case we mean.  That is
    ** what makes the ENQ this routine used to hold unnecessary rather than
    ** merely inconvenient. */
    expect = delacee;
    __cas((unsigned *)asxbsenv, (unsigned *)&expect, 0);

    if (sup) {
        __asm__("\n"
"*\n"
"* we're in supervisor state, switch back to callers key\n"
"*\n"
"         SPKA\t0(%0)" : : "r"(key));
    }
    else {
        __asm__("\n"
"*\n"
"* return to problem state\n"
"*\n"
"         MODESET KEY=NZERO,MODE=PROB\n" : : : "1", "14", "15");
    }

    *acee = (ACEE*)0;
    return rc;
}
