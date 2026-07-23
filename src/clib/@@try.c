#include <clibstae.h>
#include <clibcrt.h>
#include <clibwto.h>

typedef struct {
    unsigned    r[16];
} REGS;
typedef struct {
    unsigned    u[2];
} PARAM;

__asm__("\n&FUNC    SETC 'failed'");
static int
failed(SDWA *sdwa, void *udata)
{
    PARAM       *param  = (PARAM*)udata;
    REGS        *regs   = (REGS*)param->u[1];
    unsigned    abcode  = (*(unsigned*)&sdwa->SDWACMPF) & 0x00FFFFFF;
    unsigned    retry   = 0;

    /* Check SDWACLUP first.  When RTM enters this exit only to clean up
     * (the task is terminating) a retry is invalid - requesting one asks
     * RTM to resume a task it is tearing down.  Emit a single WTO from a
     * pre-formatted buffer and tell RTM to continue with termination.
     * Do NO C-runtime work here (no try()/__crtget()/malloc): under a
     * terminating TCB this task's libc370 CRT may already be gone, and
     * wto() is CRT-free (SVC 35 only).
     */
    if (sdwa->SDWAERRD & SDWACLUP) {
        char    msg[] = "libc370 __try: recovery cleanup-only, retry suppressed";

        wto(msg);
        SETRP(sdwa, 0, 0, 0);   /* RC=0, continue with termination */
        return 0;
    }

    if (abcode) {
        /* return abend code in R15 */
        regs->r[15] = abcode;
    }

    /* get the retry address */
    __asm__("L\t%0,=A(RETRY)" : "=r" (retry));

    /* suppress dump */
    sdwa->SDWACMPF = 0;

    /* update the retry registers */
    __asm__("MVC\t0(64,%0),0(%1)" :
        : "r" (&sdwa->SDWASR00), "r" (regs) );

    /* RC=4,RETRY=retry,restore registers */
    SETRP(sdwa,4,retry,1);

    /* 4=retry */
    return 4;
}

__asm__("\n&FUNC    SETC 'call'");
static int
call(void *func, void *plist)
{
    int         rc;
    REGS        regs;

    /* populate the retry registers */
    __asm__("STM\t0,14,0(%0)" : : "r" (&regs));
    regs.r[15] = (unsigned) (-1);

    /* create ESTAE with failed() as the recovery routine */
    rc = estae(ESTAE_CREATE, failed, &regs);
    if (rc) {
        rc *= -1;   /* make negative */
        goto quit;
    }

    __asm__(
    "LR\t15,%0          => function to call \n\t"
    "LR\t1,%1           => parameter list\n\t"
    "BALR\t14,15         call function\n\t"
    "SR\t15,15         function completed without abend"
    :
    : "r" (func), "r" (plist)
    : "0", "1", "14", "15");

    __asm__("\n"
"RETRY    DS   0H");

    __asm__("LR\t%0,15" : "=r" (rc));

    /* remove the estae */
    estae(ESTAE_DELETE, 0, 0);

quit:
    return rc;
}

/* call func with ESTAE protection, RC0=success otherwise failed */
int
__try(void *func, ...)
{
	CLIBCRT 	*crt = __crtget();
    int         rc;
    void        *r1 = (void*)(&func)+4;

    rc = call(func, r1);

	if (crt) crt->crttryrc = rc;

    return rc;
}

unsigned __tryrc(void)
{
	unsigned 	rc = 0xFFFFFFFF;
	CLIBCRT 	*crt = __crtget();
	
	if (crt) rc = crt->crttryrc;
	
	return rc;
}
