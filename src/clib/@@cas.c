/* @@CAS.C - compare and swap, the way the instruction does it
*/
#include "clibos.h"

/*
 * S/370 CS in one call: compare *mem against *expect and store new_value only
 * if they are equal.  What makes it worth having over __swap() is the failure
 * path - the instruction leaves the CURRENT value in R0 when the comparison
 * fails, and that value is what a retry loop needs.  Handing it back through
 * *expect is the C11 atomic_compare_exchange shape:
 *
 *     unsigned want = SLOT_FREE;
 *     if (__cas(&slot, &want, mine) == 0) {
 *         ... the slot is ours ...
 *     }
 *     ... want now holds who got there first ...
 *
 * That is the operation rexx370 needed when it worked around the broken __cs()
 * by swapping a value in and swapping it back out again (irx#anch.c): between
 * those two swaps another thread sees a value that was never meant to be
 * published.  One CS has no such window.
 */
int __cas(unsigned *mem, unsigned *expect, unsigned new_value)
{
    int rc = -1;

    if (!mem || !expect) goto quit;

    /* rc is written on BOTH paths inside the asm: an "=m" output that the
       assembler only sometimes stores would let the compiler drop the C
       initialiser as dead. */
    __asm__("\n"
"         L     0,0(,%2)    expected value\n"
"         LR    1,%3        new value\n"
"         CS    0,1,0(%1)   swap it in if *mem is still the expected one\n"
"         BC    8,@@CASOK   CC=0: it was, and it is ours now\n"
"         ST    0,0(,%2)    CC=1: hand back what is there instead\n"
"         LA    1,1\n"
"         ST    1,%0        rc = 1, not swapped\n"
"         B     @@CASX\n"
"@@CASOK  SR    1,1\n"
"         ST    1,%0        rc = 0, swapped\n"
"@@CASX   DS    0H"
        : "=m"(rc) : "r"(mem), "r"(expect), "r"(new_value) : "0", "1");

quit:
    return rc;
}
