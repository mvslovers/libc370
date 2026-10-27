#ifndef MVS_RECOVERY_H
#define MVS_RECOVERY_H
#include <sys/_cc370.h>
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
    DUMP_SDUMP          /* the same as DUMP_DEFAULT (#413) */
} DUMP_OP;

/* ORed into the DUMP_OP argument of abendrpt() (#712): the report routine
** writes no "libc370 recovery: ... cleanup only" line when the task is
** being terminated (cancel, or the job step ending) and RTM runs it only
** to clean up. Every other report is unchanged. */
#define ABRPT_QUIET_TERM 0x100

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

/* abend report via WTO messages */
extern int __abrpt(ESTAE_OP op, DUMP_OP);
#define abendrpt(eop,dop) __abrpt((eop),(dop))

/* return last ___try() rc/abend code */
unsigned ___tryrc(void);
#define tryrc() ___tryrc()

/* where the last ___try() abend happened (#530): the BC-mode PSW at the
** time of error (SDWACTL1 -- instruction address, ILC, interruption code)
** and GPR 14 then (SDWAGR14: the return point of the last call the
** failing code made -- the return address into its caller only when it
** had made none, i.e. a fault in a prologue or a leaf function).
** Cleared and set exactly when tryrc() is:
** zero after a try() that did not abend, but after a try() whose function
** itself caught an abend in a nested try(), the nested one's values.
** The pointer is into the CRT and stays valid;
** the values are overwritten by the next try(). */
typedef struct tryinfo {
    unsigned    psw[2];     /* BC-mode PSW at the time of error         */
    unsigned    r14;        /* GPR 14 at the time of error              */
} TRYINFO;

extern const TRYINFO *__tryinf(void);
#define tryinfo() __tryinf()

#endif /* MVS_RECOVERY_H */
