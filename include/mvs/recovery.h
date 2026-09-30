#ifndef MVS_RECOVERY_H
#define MVS_RECOVERY_H
/* mvs/recovery.h - recovery: ESTAE and try().
**
** libc370 2.0 merges clibstae.h and clibtry.h into this header (#256).
** Their declarations are unchanged and appear in dependency order.
*/

/* ---- 1.x clibstae.h -------------------------------------------------- */
#include <ibm/mvs/ihasdwa.h>

typedef enum {
    ESTAE_CREATE,
    ESTAE_OVERLAY,
    ESTAE_DELETE
} ESTAE_OP;

typedef enum {
    DUMP_SUPPRESS,
    DUMP_DEFAULT,
    DUMP_SNAP,
    DUMP_SDUMP
} DUMP_OP;

#define SETRP(sdwa,rc,retry,regs) \
    ((sdwa)->SDWARCDE=(rc), \
    (sdwa)->SDWARTYA=(retry), \
    (sdwa)->SDWAACF2|=((regs)?SDWAUPRG:0))

extern int __estae(ESTAE_OP op, void *fp, void *udata);
#define estae(op,fp,udata) __estae((op),(fp),(udata))

/* ---- 1.x clibtry.h --------------------------------------------------- */
/* call func with ESTAE protection, RC0=success otherwise failed */
/* note: used when we want the failure reason returned in rc
** 	when rc is negative value the ESTAE CREATE failed. 
**  otherwise rc is a 0x00sssuuu formatted value. 
**  sss is system abend code, uuu is user abend code.
*/
extern int ___try(void *func, ...);
#define try(func,...) ___try((func), __VA_ARGS__)

/* call func with ESTAE protection. Returns 0 unless the ESTAE create fails */
/* note: used only when we don't care the failure reason */
extern int __try(void *func, ...);

/* abend report via WTO messages */
extern int __abrpt(ESTAE_OP op, DUMP_OP);
#define abendrpt(eop,dop) __abrpt((eop),(dop))

/* return last ___try() or __try() rc/abend code */
unsigned ___tryrc(void);
#define tryrc() ___tryrc()

/* return last ___try() or __try() rc/abend code */
unsigned __tryrc(void);

#endif /* MVS_RECOVERY_H */
