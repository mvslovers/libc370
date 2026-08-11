/* @@PPAHRV.C - __ppahrv() - free what a dead program's own exit path
 * would have freed (#96).
 *
 * Called from ___try()/__try()'s abend-path walk (#93) for each
 * validated abandoned PPA, BEFORE the stack+PPA block itself is
 * FREEMAINed.  A program that abends under try() never runs __exit()
 * or @@EXITA: its eagerly opened stdio FILEs (@@start.c fopens
 * *SYSPRINT, *SYSTERM and SYSIN for every C program) stay OPEN with
 * DCBs and buffers - deliberately pinned outside the ambient heap
 * subpool, so no subpool release can ever reclaim them.  Measured in
 * the #96 T7 probe (tstcrtlk): ~132K per caught abend, on top of the
 * ~40K of ambient-subpool heap and the 262K stack #93 reclaims.
 *
 * This mirrors __exit() (@@exit.c) on the DEAD program's CLIBGRT,
 * with two deliberate differences:
 *   - the atexit()/on_exit() functions are NOT run - executing the
 *     dead program's code inside abend recovery is how a leak would
 *     become a crash; their registration arrays are just freed.
 *   - the CLIBGRT and the CLIBCRTs are freed afterwards, which
 *     @@GRTRES/@@CRTRES would have done from @@EXITA.
 *
 * Every hop validates before it trusts: 24-bit pointer checks, the
 * "CLIBGRT "/"CLIBCRT " eyecatchers, never the survivor's own GRT,
 * and fclose() validates each FILE's eyecatcher itself.  Anything
 * that does not validate is left alone - a leak, not a corruption.
 * CLOSE of the dead program's DCBs is legal here: same TCB, and the
 * task is not terminating.  This runs AFTER the #93 walk unhooked
 * 8(TCBFSAB), so every CRT-anchored call resolves through the
 * surviving caller's runtime.
 */
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "clibppa.h"
#include "clibary.h"

void
__ppahrv(CLIBPPA *ppa)
{
    CLIBGRT     *grt;
    unsigned    count;

    if (!ppa) return;

    grt = (CLIBGRT *)ppa->ppagrt;
    if (grt && (unsigned)grt <= 0x00FFFFFF
            && grt != __grtget()
            && memcmp(grt->grteye, "CLIBGRT ", 8) == 0) {

        /* registration arrays only - the functions are NOT run */
        if (grt->grtexit)  arrayfree(&grt->grtexit);
        if (grt->grtexita) arrayfree(&grt->grtexita);

        /* close the dead program's files: the 132K.  fopen()
           registers every FILE here including stdin/stdout/stderr,
           so the array is the complete set (as __exit() relies on) */
        if (grt->grtfile) {
            count = arraycount(&grt->grtfile);
            while (count > 0) {
                FILE    *fp;

                count--;
                fp = grt->grtfile[count];
                grt->grtfile[count] = 0;
                if (fp && (unsigned)fp <= 0x00FFFFFF) {
                    fclose(fp);
                }
            }
            arrayfree(&grt->grtfile);
        }
        grt->grtin  = 0;
        grt->grtout = 0;
        grt->grterr = 0;

        if (grt->grtenv) {
            count = arraycount(&grt->grtenv);
            while (count > 0) {
                count--;
                if (grt->grtenv[count]) free(grt->grtenv[count]);
            }
            arrayfree(&grt->grtenv);
        }

        if (grt->grtwsa) {
            count = arraycount(&grt->grtwsa);
            while (count > 0) {
                count--;
                if (grt->grtwsa[count]) free(grt->grtwsa[count]);
            }
            arrayfree(&grt->grtwsa);
        }

        if (grt->grtdevtb) {
            count = arraycount(&grt->grtdevtb);
            while (count > 0) {
                count--;
                if (grt->grtdevtb[count]) free(grt->grtdevtb[count]);
            }
            arrayfree(&grt->grtdevtb);
        }

        if (grt->grtptrs) {
            /* the pointers in this array belong to the caller */
            arrayfree(&grt->grtptrs);
        }

        memset(grt->grteye, 0, sizeof(grt->grteye));
        free(grt);
    }
    ppa->ppagrt = 0;

    /* the dead task-level CRTs, as @@CRTRES would have */
    if (ppa->ppacrt && (unsigned)ppa->ppacrt <= 0x00FFFFFF) {
        count = arraycou(&ppa->ppacrt);
        while (count > 0) {
            CLIBCRT     *crt;

            count--;
            crt = ppa->ppacrt[count];
            if (crt && (unsigned)crt <= 0x00FFFFFF
                    && memcmp(crt->crteye, "CLIBCRT ", 8) == 0) {
                memset(crt->crteye, 0, sizeof(crt->crteye));
                free(crt);
            }
        }
        arrayfree(&ppa->ppacrt);
    }
}
