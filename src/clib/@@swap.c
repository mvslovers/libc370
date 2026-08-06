/* @@SWAP.C - atomic exchange
*/
#include "clibos.h"

/*
 * Store new_value and return what was there, atomically.  This is what the
 * function formerly called __cs() actually did: the CS retry loop turns the
 * instruction's compare into an unconditional exchange, because the caller
 * never gets to say what it expected.  Under its old name it also promised
 * compare-and-swap semantics it could not deliver - see __cas() for those.
 *
 * mem must not be NULL.  It is checked, and 0 is returned for a NULL, which
 * is indistinguishable from a successful exchange that found 0 - so treat the
 * check as a guard against abending, not as an error report.
 */
unsigned __swap(unsigned *mem, unsigned new_value)
{
    unsigned  old_value = 0;

    if (!mem) goto quit;

    /* %2 holds new_value ITSELF, so it is moved with LR.  It used to be
       loaded from - `L 1,0(,%2)` - which stored the word at the address
       new_value happened to look like (#48).

       R0 and R1 are written here and have to be declared.  The retry label is
       function-specific because a label in inline assembler is file-scope in
       the generated source. */
    __asm__("\n"
"@@SWPAGN DS    0H\n"
"         L     0,0(,%1)    get current value\n"
"         LR    1,%2        get new value\n"
"         CS    0,1,0(%1)   save new value in memory\n"
"         BNZ   @@SWPAGN    changed under us, try again\n"
"         ST    0,%0        return the value we replaced"
        : "=m"(old_value) : "r"(mem), "r"(new_value) : "0", "1");

quit:
    return old_value;
}
