#include <clibstae.h>
#include <clibcrt.h>
#include <clibppa.h>
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
     *
     * #9: mirror of the guard in @@try.c failed().  This copy - @@@try.c,
     * reached via try() -> ___try() - is the one every consumer actually
     * drives; the @@try.c copy (__try) is not called by anyone. */
    if (sdwa->SDWAERRD & SDWACLUP) {
        char    msg[] = "libc370 @@@try.c failed(): recovery cleanup-only, retry suppressed";

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
	CLIBCRT 	*crt = __crtget();
    int         rc;
    REGS        regs;
    unsigned    *fsanext = 0;
    unsigned    savenext = 0;

    /* Snapshot the TCB first-save-area "next" word.  If func LINKs a C
     * program that abends, that program's @@CRT0 pushed its PPA there
     * and its @@EXITA never ran to pop it: after the retry every
     * CRT-anchored libc call (stdio, __crtget, the ambient heap
     * subpool) would resolve through the DEAD program's environment -
     * measured as an S0C4 in the #89 T4 probe (tstsplnk).  Restore the
     * word on the abend path; the abandoned PPA's owner is gone. */
    {
        unsigned    *psa = 0;                       /* low core == PSA */
        unsigned    tcb  = psa[0x21C / 4];          /* PSATOLD         */
        unsigned    fsa  = *(unsigned *)(tcb + 0x70) & 0x00FFFFFF;

        if (fsa) {
            fsanext  = (unsigned *)(fsa + 8);
            savenext = *fsanext;
        }
    }

    /* populate the retry registers */
    __asm__("STM\t0,14,0(%0)" : : "r" (&regs));
    regs.r[15] = (unsigned) (-1);

	if (crt) crt->crttryrc = 0;

    /* create ESTAE with failed() as the recovery routine */
    rc = estae(ESTAE_CREATE, failed, &regs);
    if (rc) {
        rc *= -1;   /* make negative */
        goto quit;
    }

    __asm__(
    "LR\t15,%1          => function to call \n\t"
    "LR\t1,%2           => parameter list\n\t"
    "BALR\t14,15         call function\n\t"
    "SR\t15,15         function completed without abend\n\t"
    "LR\t%0,15         save return code"
    : "=r" (rc)
    : "r" (func), "r" (plist)
    : "0", "1", "14", "15");

	if (!rc) goto cleanup;

    __asm__("\n"
"RETRY    DS   0H");

    __asm__("LR\t%0,15" : "=r" (rc));

    /* abend path: unhook whatever a dead LINKed program left chained
       at 8(TCBFSAB) before any CRT-anchored libc runs (see above),
       then free it (#93).  A dead program's @@EXITA never ran, so the
       stack+PPA block its @@CRT0 GETMAINed (~262K) is orphaned: its
       RB was purged by RTM before the retry and nothing else points
       at the block.  Walk PPASAVE from the abandoned head back to the
       snapshot so a dead program that itself LINKed a dead program
       releases the whole chain, validating every hop the way @@PPAGET
       does - a garbage value at 8(TCBFSAB) must free nothing.  Unhook
       FIRST: a second abend re-enters RETRY with the chain already
       popped and walks nothing.  FREEMAIN is conditional (RC): inside
       abend recovery a bad request must be a return code, not another
       abend.  No CRT-anchored calls here; wto() is SVC 35 only. */
    if (fsanext) {
        unsigned    dead  = *fsanext;
        unsigned    depth = 0;

        *fsanext = savenext;

        while (dead && dead != savenext && dead <= 0x00FFFFFF
               && depth++ < 16) {              /* depth: cycle guard */
            CLIBPPA     *ppa = (CLIBPPA *)dead;
            unsigned    lv, sp, frc;

            if (*(unsigned *)ppa->ppaeye !=
                ((unsigned)'@' << 24 | (unsigned)'P' << 16 |
                 (unsigned)'P' << 8  | (unsigned)'A')) break;
            lv = ppa->ppastkln;                /* whole block SP||LV,  */
            sp = (unsigned char)ppa->ppasubpl; /* as @@EXITA frees it  */
            if (!lv || lv > 0x00FFFFFF) break;

            dead = (unsigned)ppa->ppasave;     /* read before the free */
            __asm__("FREEMAIN RC,A=(%1),LV=(%2),SP=(%3)\n\t"
                    "LR\t%0,15"
                    : "=r"(frc)
                    : "r"(ppa), "r"(lv), "r"(sp)
                    : "0", "1", "14", "15");
            if (frc) {
                char    msg[] = "libc370 @@@try.c call(): FREEMAIN of an abandoned PPA failed, walk stopped";

                wto(msg);
                break;
            }
        }
    }

	/* isolate the system and user abend codes */
    rc &= 0xFFFFFF;
    
    /* save abend code in crt */
	if (crt) crt->crttryrc = rc;

cleanup:
    /* remove the estae */
    estae(ESTAE_DELETE, 0, 0);

quit:
    return rc;
}

/* call func with ESTAE protection, RC0=success otherwise failed */
int
___try(void *func, ...)
{
    int         rc;
    void        *r1 = (void*)(&func)+4;

    rc = call(func, r1);

#if 0	/* debug code */
	if (rc != 0) {
		if (rc > 0xFFF) {
			/* ESTAE capyured system abend */
			wtof("%s: ABEND S%03X", __func__, (rc >> 12) & 0xFFF);
		}
		else if (rc > 0) {
			/* ESTAE captured user abend */
			wtof("%s: ABEND U%04d", __func__, rc);
		}
		else if (rc < 0) {
			/* ESTAE CREATE failed in call() function */
			int i = rc *= -1;	/* make rc a positive value */
			wtof("%s: ESTAE CREATE failed, RC=0x%08X (%d)", __func__, i, i);
		}
	}
#endif

    return rc;
}

unsigned ___tryrc(void)
{
	unsigned 	rc = 0xFFFFFFFF;
	CLIBCRT 	*crt = __crtget();
	
	if (crt) rc = crt->crttryrc;
	
	return rc;
}
