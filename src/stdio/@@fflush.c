/* @@FFLUSH.C - caller should already hold lock on file handle */
#include "src/internal/fileio.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include "src/internal/bsam.h"
#include <ibm/mvs/dcbd.h>
#include <ibm/mvs/ihadecb.h>

#define begwrite(fp, len)   (lenwrite = (len), dptr = (fp)->asmbuf)
#define finwrite(fp)        (__awrite((fp)->dcb, &dptr, &lenwrite))

static int flushrec(FILE *fp, int newline);
static int updrec(FILE *fp);
static int fixflush(FILE *fp, int fill);
static int varflush(FILE *fp);
static int tso_putline(char *buf, unsigned len);

/* __fflush() - write what is pending; with nothing pending, write nothing.
   This is what fflush(), fclose(), fseek() and freopen() need. */
int
__fflush(FILE *fp)
{
    return flushrec(fp, 0);
}

/* __fflnl() - a '\n' on a text stream.  A newline asks for a record whether
   or not it carries data, so an empty buffer here is an empty line and
   still becomes a record (#199).  Before this, __fputc() called __fflush()
   and the empty line was dropped: fputs("a\n\nb\n") wrote two records,
   measured on mvsdev JOB00495 on FB 80 and VB 84. */
int
__fflnl(FILE *fp)
{
    return flushrec(fp, 1);
}

__asm__("\n&FUNC    SETC 'flushrec'");
static int
flushrec(FILE *fp, int newline)
{
	DCB				*dcb 	= fp->dcb;
    int             err     = 0;
    unsigned char   *dptr;
    size_t          lenwrite;
    int             fill;

    /* a record overwritten in place goes back through the UPDAT DCB
       it was read from (#189) - that DCB reads, so it comes first */
    if (fp->xflags & _FILE_XFLAG_DIRTY) {
        if (fp->flags & _FILE_FLAG_ERROR) {
            /* fail fast (#149): a rewrite that failed is not re-driven */
            fp->xflags &= ~_FILE_XFLAG_DIRTY;
            if (fp->flags & _FILE_FLAG_ENOSPC) { err = 12; errno = ENOSPC; }
            else                               { err =  8; errno = EIO;    }
            goto quit;
        }
        err = updrec(fp);
        goto quit;
    }

    if (fp->flags & _FILE_FLAG_RECORD)   goto quit; /* not using buffer  */
    if (!(fp->flags & _FILE_FLAG_DCBOUT)) goto quit; /* DCB does not write */
    if (fp->upto == fp->buf && !newline) goto quit; /* nothing pending   */

    /* Fail fast (#149).  Re-driving a stream that has already failed
       cannot succeed, and what is in the buffer is undeliverable either
       way - discard it here rather than carry it into the next flush.
       With the guards in @@fwrite.c and @@fputc.c nothing should reach
       this with a non-empty buffer; it is here so that no path can. */
    if (fp->flags & _FILE_FLAG_ERROR) {
        if (fp->flags & _FILE_FLAG_ENOSPC) { err = 12; errno = ENOSPC; }
        else                               { err =  8; errno = EIO;    }
        goto reset;
    }

	/* This is new code to support TSO terminals */
	if (!(fp->xflags & _FILE_XFLAG_PUTLINE) && dcb->dcbdevt == DCBDVTRM) {
		/* DCB refers to terminal */
		DECB	decb = {0};
		size_t  len;

		/* calculate length of data in buffer */
		len = fp->upto - fp->buf;

		err = tso_putline(fp->buf, len);
		goto reset;
	}


    switch(fp->recfm & _FILE_RECFM_TYPE) {
    case _FILE_RECFM_F: /* RECFM=F..    */
    case _FILE_RECFM_U: /* RECFM=U      */
        if (fp->flags & _FILE_FLAG_BINARY) {
            fill = 0;
        }
        else {
            fill = ' ';
        }
        err = fixflush(fp, fill);
        break;
    case _FILE_RECFM_V: /* RECFM=V..    */
        err = varflush(fp);
        break;
    }

    if (err) {
        /* uncorrectable I/O error on the physical write, recorded by
           the SYNAD exit instead of ABEND S001 (#147); the buffer is
           still reset below - the block is undeliverable.
           err==12 is the x37 exit (#176): the data set is out of
           space, which is the caller's problem to solve and not a
           device error, so it gets its own errno */
        fp->flags |= _FILE_FLAG_ERROR;
        if (err == 12) fp->flags |= _FILE_FLAG_ENOSPC;
        errno = (err == 12) ? ENOSPC : EIO;
    }
    else if (!(fp->flags & _FILE_FLAG_BINARY)) {
        /* The byte view (#189): a text record reads back as its data -
           on F padded with blanks to LRECL, on U an empty line is the
           one blank fixflush() wrote - followed by '\n'.  Count what was
           written the same way, or fseek(ftell()) would not land where
           a reader of the same data set is: after "L1\n" on FB 80 the
           position is 81, not 3.  A flush without a '\n' still ends the
           record, which reads back with one. */
        size_t  len = fp->upto - fp->buf;
        long    pad = 0;

        switch(fp->recfm & _FILE_RECFM_TYPE) {
        case _FILE_RECFM_F:
            if (len < (size_t)fp->lrecl) pad = (long)(fp->lrecl - len);
            break;
        case _FILE_RECFM_U:
            if (len == 0) pad = 1;
            break;
        }
        fp->filepos += pad + (newline ? 0 : 1);
    }

reset:
    /* reset buffer.  filepos is NOT reset: it is the position in the
       file, not in the record, and ftell() returns it (#200) */
    fp->upto = fp->buf;

quit:
    return err;
}

/* updrec() - hand the record in fp->buf, changed in place, to __awrite()
 * (#189).  On an UPDAT DCB __awrite() replaces the record __aread() last
 * returned - which is the one in fp->buf, because __fgetc() commits
 * before it reads the next - and the block goes back to the disk when
 * the next block is read or at CLOSE.  The record keeps its length: F is
 * padded to LRECL (text with blanks), V and U go back as long as they
 * came.  filepos does not move.
 */
__asm__("\n&FUNC    SETC 'updrec'");
static int
updrec(FILE *fp)
{
    int             err;
    unsigned char   *dptr   = fp->asmbuf;
    size_t          len     = fp->endbuf - fp->buf;
    size_t          lenwrite;

    fp->xflags &= ~_FILE_XFLAG_DIRTY;

    /* text records carry the '\n' __fgetc() added; it is not data */
    if (!(fp->flags & _FILE_FLAG_BINARY) && len > 0) len--;

    switch(fp->recfm & _FILE_RECFM_TYPE) {
    case _FILE_RECFM_F:
        memset(dptr, (fp->flags & _FILE_FLAG_BINARY) ? 0 : ' ', fp->lrecl);
        memcpy(dptr, fp->buf, len);
        lenwrite = fp->lrecl;
        break;
    case _FILE_RECFM_V:
        dptr[0] = (unsigned char)((len + 4) >> 8);
        dptr[1] = (unsigned char)((len + 4) & 0xFF);
        dptr[2] = 0;
        dptr[3] = 0;
        memcpy(dptr + 4, fp->buf, len);
        lenwrite = len + 4;
        break;
    default:
        memcpy(dptr, fp->buf, len);
        lenwrite = len;
        break;
    }

    err = __awrite(fp->dcb, &dptr, &lenwrite);
    if (err) {
        fp->flags |= _FILE_FLAG_ERROR;
        if (err == 12) fp->flags |= _FILE_FLAG_ENOSPC;
        errno = (err == 12) ? ENOSPC : EIO;
    }
    return err;
}

__asm__("\n&FUNC    SETC 'fixflush'");
static int
fixflush(FILE *fp, int fill)
{
    int             err     = 0;
    unsigned char   *dptr;
    size_t          lenwrite;
    size_t          len;

    /* calculate length of data in buffer */
    len = fp->upto - fp->buf;

    /* fill and copy data to internal buffer */
    if ((fp->recfm & _FILE_RECFM_TYPE)==_FILE_RECFM_U) {
        /* RECFM=U, no fill, just copy */
        if (len == 0) {
            /* an empty line (#199): a zero-length block cannot be
               written, so it becomes one fill byte */
            begwrite(fp, 1);
            *dptr = (unsigned char)fill;
        }
        else {
            begwrite(fp, len);
            memcpy(dptr, fp->buf, len);
        }
    }
    else {
        /* RECFM=F, fill and copy */
        begwrite(fp, fp->lrecl);
        memset(dptr, fill, fp->lrecl);
        memcpy(dptr, fp->buf, len);
        len = fp->lrecl;
    }

    /* write buffer to disk */
    err = finwrite(fp);

quit:
    return err;
}

__asm__("\n&FUNC    SETC 'varflush'");
static int
varflush(FILE *fp)
{
    int             err     = 0;
    unsigned char   *dptr;
    size_t          lenwrite;
    size_t          len;
    size_t          rdwlen;

    /* calculate length of data in buffer */
    len = fp->upto - fp->buf;

	rdwlen = len+4;
	begwrite(fp, rdwlen);
	/* copy data to internal buffer */
	memcpy(dptr+4, fp->buf, len);

	/* calculate the RDW */
	dptr[0] = (unsigned char) (rdwlen >> 8);
	dptr[1] = (unsigned char) (rdwlen & 0xFF);
	dptr[2] = 0;
	dptr[3] = 0;

	/* write buffer to disk */
	err = finwrite(fp);

    // wtof("@@fflush:%s: dptr=0x%08X lenwrite=%u err=%d", __func__, dptr, lenwrite, err);
    // wtodumpf(dptr, lenwrite, "dptr");

quit:
    return err;
}

static int
tso_putline(char *buf, unsigned len)
{
	unsigned	r0 = 0;
	unsigned	r1 = 0;

	// wtof("%s: enter", __func__);
	if (!len) goto quit;
	
	// wtodumpf(buf, len, "@@fflush.c:%s: buf", __func__);
	
#define TPUT_WAIT		0x00
#define TPUT_NOWAIT		0x10
#define TPUT_HOLD		0x08
#define TPUT_BREAKIN	0x04
#define TPUT_CONTROL	0x02
#define TPUT_ASIS		0x01
#define TPUT_EDIT		0x00
#define TPUT_FULLSCR	(TPUT_CONTROL|TPUT_ASIS)

	r0 = len & 0xFFFF;
	r1 = (TPUT_WAIT | TPUT_HOLD) << 24 | (unsigned) buf & 0x00FFFFFF;

	__asm__(
		"LR\t0,%0                buffer length\n\t"
		"LR\t1,%1                flags and buffer address\n\t"
		"TPUT\t(1),(0),R" : : "r"(r0), "r"(r1)  : "0", "1", "14", "15", "memory");

quit:
	// wtof("%s: exit rc=%d", __func__, 0);
	return 0;
}
