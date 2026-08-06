/* @@CS.C - compare and swap
*/
#include "clibos.h"

unsigned __cs(void *mem, unsigned new_value)
{
    unsigned  old_value = 0;

    if (!mem) goto quit;

    /* %2 holds new_value ITSELF, so it is moved with LR.  It used to be
       loaded from - `L 1,0(,%2)` - which stored the word at the address
       new_value happened to look like: nothing useful when that was low
       storage, an S0C4 when it was protected (#48).

       R0 and R1 are written here and have to be declared; the asm named
       neither.  The retry label is function-specific because a label in
       inline assembler is file-scope in the generated source.            */
    __asm__("\n"
"@@CSAGN  DS    0H\n"
"         L     0,0(,%1)    get current value\n"
"         LR    1,%2        get new value\n"
"         CS    0,1,0(%1)   save new value in memory\n"
"         BNZ   @@CSAGN     changed, try again\n"
"         ST    0,%0        return value"
        : "=m"(old_value) : "r"(mem), "r"(new_value) : "0", "1");

quit:
    return old_value;
}
