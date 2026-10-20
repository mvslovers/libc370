#ifndef SRC_INTERNAL_TSKEY_H
#define SRC_INTERNAL_TSKEY_H
/* src/internal/tskey.h - internal: the task's own PSW key for a stretch
** of code that creates state the library keeps (#693).
**
** In supervisor state with PSW key 0 GETMAIN takes subpool 0 from subpool
** 252, key-0 storage (IEAVGM00, PGTSPQE: key 0 and supervisor state
** both).  What the library creates on first use - the standard streams,
** the SYSENV environment - would then be key 0 if the first use came
** inside a key-0 window (__super(PSWKEY0, ...)), and the program, back
** in its own key, ends S0C4 storing into it or S30A freeing it.
** tskey() switches a supervisor-state caller whose PSW key is not the
** TCB's to the TCB's key (TCBPKF); subpool 0 is then the task's own.
** In problem state the PSW key is the TCB's already, and nothing is done:
**
**     unsigned st = tskey();     ... the task's key ...
**     tsrestore(st);
**
** Supervisor state stays as it was; only the key changes, by SPKA. */
#include "mvs/apf.h"

/* bit 8 set: the key was switched, bits 0-7 the caller's PSW key */
static __inline unsigned tskey(void)
{
    const unsigned      *psatold = (const unsigned *)0x21C; /* PSATOLD */
    const unsigned char *tcb;
    unsigned char       key;

    if (!__issup()) return 0;
    tcb = (const unsigned char *)*psatold;
    __asm__("IPK\t0             get psw key in R2\n\t"
            "STC\t2,0(,%0)      save it"
            : : "r"(&key) : "2", "memory");
    if (key == tcb[0x1C]) return 0;                         /* TCBPKF */
    __asm__("IC\t2,0(,%0)       the task's key\n\t"
            "SPKA\t0(2)         into the psw"
            : : "r"(&tcb[0x1C]) : "2", "memory");
    return 0x100 | key;
}

static __inline void tsrestore(unsigned st)
{
    unsigned char key = (unsigned char)st;

    if (!(st & 0x100)) return;
    __asm__("IC\t2,0(,%0)       the caller's psw key\n\t"
            "SPKA\t0(2)         back into the psw"
            : : "r"(&key) : "2", "memory");
}

#endif /* SRC_INTERNAL_TSKEY_H */
