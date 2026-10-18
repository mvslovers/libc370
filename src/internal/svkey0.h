#ifndef SRC_INTERNAL_SVKEY0_H
#define SRC_INTERNAL_SVKEY0_H
/* src/internal/svkey0.h - internal: supervisor state and key 0 for a
** stretch of code, and back to exactly the caller's state (#402).
**
** MODESET KEY=ZERO,MODE=SUP and then KEY=NZERO,MODE=PROB returned every
** caller in problem state with the TCB's key - also one that came in
** supervisor state or key 0, whose next privileged instruction then
** failed S0C2.  svkey0() remembers which it was:
**
**     unsigned st = svkey0();     ... supervisor state, key 0 ...
**     svrestore(st);
**
** The caller must be APF authorized when it is in problem state (MODESET
** is SVC 107 and abends S047 otherwise); check __isauth() first. */
#include "mvs/apf.h"

/* bit 8 set: the caller was in supervisor state, bits 0-7 its PSW key */
static __inline unsigned svkey0(void)
{
    unsigned char key;

    if (__issup()) {
        __asm__("IPK\t0             get psw key in R2\n\t"
                "STC\t2,0(,%0)      save it\n\t"
                "SLR\t2,2           key 0\n\t"
                "SPKA\t0(2)         into the psw"
                : : "r"(&key) : "2", "memory");
        return 0x100 | key;
    }
    __asm__("MODESET KEY=ZERO,MODE=SUP" : : : "0", "1", "14", "15", "memory");
    return 0;
}

static __inline void svrestore(unsigned st)
{
    unsigned char key = (unsigned char)st;

    if (st & 0x100) {
        __asm__("IC\t2,0(,%0)       the caller's psw key\n\t"
                "SPKA\t0(2)         back into the psw"
                : : "r"(&key) : "2", "memory");
        return;
    }
    __asm__("MODESET KEY=NZERO,MODE=PROB" : : : "0", "1", "14", "15", "memory");
}

#endif /* SRC_INTERNAL_SVKEY0_H */
