/* @@FABAND.C - __fabandon(): close a FILE whose last write failed (#168).
**
** fclose() flushes before it closes.  When the pending block is exactly what
** could not be written, that re-drives the failing WRITE and the close abends
** in turn - and it takes the DD with it.  Measured on mvsdev 2026-09-09 with
** an FTP upload into SPACE=TRK(1,0) (mvslovers/ftpd#129):
**
**     IEC031I D37-04,IFG0554T,FTPDT,FTPDT,SYS00006,...
**     FTPD070E ABEND SD37 RECOVERED CMD=STOR SOCKET=3 TOTAL=1
**     FTPD076W CLOSE ABENDED AFTER STOR, DD=SYS00006 FREE RC=4
**
** fclose() enters and does not return, so __fpfree() never runs; __dsfree()
** on that DD by name answers 4 because the DCB the failed CLOSE left open
** still holds the allocation; and the data set stays allocated to the address
** space for the life of the job - IDCAMS DELETE answers 8 from inside it.  A
** server that runs out of space on a data set it created cannot clean it up
** and cannot let its user clean it up either.  Only a restart releases it.
**
** Three things have to be true for the close to go through:
**
**  1. the C buffer is DISCARDED, not flushed.  This is the half that bites
**     ftpd: the D37 fires inside __awrite(), the caller's ESTAE unwinds
**     through libc370, __fflush()'s reset: label never runs, and fp->upto
**     still points past the block that failed.  fclose() then re-drives the
**     identical WRITE.
**  2. the DCB says nothing is pending - __adisc(), see asm/@@adisc.asm.  For
**     the abend shape above @@ATROUT has already cleared IOFLDATA (it resets
**     the flag BEFORE the WRITE), but that is a property of one failure and
**     not a contract; a caller that abandons with a good partial block
**     pending means "write nothing more".
**  3. CLOSE may abend anyway and must be reported, not propagated.  BSAM
**     CLOSE writes the EOF mark and can reach EOV - and a _FILE_FLAG_RECORD
**     caller has no C buffer at all, so CLOSE is the only place it can fail.
**     Hence try().
**
** This cannot be driven off _FILE_FLAG_ERROR: an x37 is an ABEND, not a SYNAD
** condition, so @@fflush.c never sets the flag (#147) and after the caller's
** ESTAE recovers the FILE looks healthy to the library.  Only the caller
** knows, which is why this is an API and not a smarter fclose().
**
** RETURNS
**    0   nothing was written, CLOSE completed, the DD was released
**   >0   CLOSE abended: ___try()'s 0x00sssuuu code (D37 reads 0x00D37000).
**        The FILE, its buffer and its heap are gone; the DD is NOT - the
**        failed CLOSE still holds the allocation, and only ending the
**        address space releases it
**   -1   not a FILE (NULL, or the eye catcher does not match)
**   -2   CLOSE completed but the DD could not be unallocated
**   -3   ESTAE CREATE failed, so CLOSE was NOT attempted.  Nothing was torn
**        down and the FILE is untouched - the caller may retry
*/
#include <stdio.h>
#include <string.h>
#include <mvssupa.h>
#include <clibtry.h>
#include "cliblock.h"

int
__fabandon(FILE *fp)
{
    int     abend   = 0;
    int     rc      = 0;

    if (!fp) return -1;
    if (strcmp(fp->eye, _FILE_EYE)!=0) return -1;

    /* Unlike fclose(), the return code is not kept and the unlock below is
       unconditional.  rc=8 means THIS TASK already holds the ENQ - and in the
       case this function exists for, that hold is the stale one from the
       fwrite() that abended: it took the FILE lock and the caller's ESTAE
       retry never DEQ'd it.  Leaving it would park a CLIBLOCK ENQ on storage
       free()d two lines further down, where the next FILE that malloc()s the
       same address either loses exclusion or waits on it forever.  There is
       no legitimate outer holder of a FILE that is being destroyed. */
    lock(fp,0);

    /* (1) discard - deliberately NOT __fflush() */
    if (fp->buf) fp->upto = fp->buf;
    fp->filepos = 0;

    if (fp->flags & _FILE_FLAG_OPEN) {
        /* (2) the DCB's own buffer state says there is nothing to write,
               so @@ACLOSE's opening FIXWRITE finds nothing to do */
        __adisc(fp->dcb);

        /* (3) CLOSE under ESTAE */
        abend = try(__aclose, fp->dcb);

        if (abend < 0) {
            /* ESTAE CREATE failed, __aclose() was never entered.  Tearing
               the FILE down now would drop fp->dcb with the DCB still open
               and make the leak permanent with nothing left to retry
               from - so change nothing and say so. */
            unlock(fp,0);
            return -3;
        }

        fp->flags  &= ~_FILE_FLAG_OPEN;
        fp->dcb     = 0;
        fp->asmbuf  = 0;
    }

    /* buffer, DD, grtfile, FILE, lock - shared with fclose() */
    rc = __fpterm(fp, 1);

    if (abend) return abend;        /* the DD is still there; say why    */
    if (rc)    return -2;           /* CLOSE was clean, SVC 99 was not   */

    return 0;
}
