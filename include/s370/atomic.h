#ifndef S370_ATOMIC_H
#define S370_ATOMIC_H
#include <sys/_cc370.h>
/* s370/atomic.h - atomic updates of a word: compare and swap (CS).
**
** libc370 2.0 splits this out of clibos.h (#256).
*/

/* __cas() - S/370 compare and swap: store new_value only if *mem is still
 * *expect.  Returns 0 when the swap happened, 1 when it did not - and then
 * *expect holds what is in *mem instead, which is what a retry loop needs:
 *
 *     unsigned want = SLOT_FREE;
 *     if (__cas(&slot, &want, mine) == 0) { ... the slot is ours ... }
 *
 * Returns -1 for a NULL mem or expect.  */
int __cas(unsigned *mem, unsigned *expect, unsigned new_value);

/* __swap() - atomic exchange: store new_value, return what was there.  mem
 * must not be NULL; a NULL returns 0, which a successful exchange finding 0
 * also does, so the check guards against abending rather than reporting.
 *
 * Replaced __cs(), which did this while promising compare-and-swap: the CS
 * retry loop it used turns the instruction's compare into an unconditional
 * exchange.  Use __cas() when the comparison is the point.  */
unsigned __swap(unsigned *mem, unsigned new_value);

/* __uinc() unsigned increment of memory, returns old value, wraps new value to 0 if old value is max unsigned */
unsigned __uinc(void *mem);

/* __udec() unsigned decrement of memory, returns old value, wraps new value to max unsigned if old value is 0 */
unsigned __udec(void *mem);

/* __inc() signed increment of memory, returns old value, wraps new value to 0 if old value is max signed */
int __inc(void *mem);

/* __dec() signed decrement of memory, returns old value, wraps new value to max signed if old value is min signed */
int __dec(void *mem);

#endif /* S370_ATOMIC_H */
