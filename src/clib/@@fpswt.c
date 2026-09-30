/* @@FPSWT.C - caller should hold lock on file handle */
#include <fileio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <bsam.h>

extern int  __fpopen(FILE *fp);

static int  redcb(FILE *fp, int out);
static void skipto(FILE *fp, long pos);

/* __fpswt() - turn a '+' stream's DCB round (#189).
 *
 * A stream opened "r+", "w+" or "a+" may read and write, but its DCB is
 * opened one way at a time (_FILE_FLAG_DCBOUT).  A read on a writing DCB,
 * or a write on a reading one, lands here: the DCB is closed and the SAME
 * DD is opened again the other way.  Nothing is allocated, so DISP and
 * the data set stay as they are, and an output open after the first is
 * EXTEND (_FILE_FLAG_EXTEND, set by __fpopen()), so a switch never
 * truncates.
 *
 * out == 0, to reading: pending output is flushed, the DD reopens for
 *   input and the stream skips forward to where it was - O(n), the price
 *   of a backward seek.  "a+" right after fopen() (_FILE_FLAG_POSEND)
 *   reads to the end instead: that is its position, and fopen() counts it
 *   this way before anything can be pending.
 *
 * out != 0, to writing: only at the end of the data set for now.  "a+"
 *   always writes there, whatever its read position - it reads on to the
 *   end first, so its position stays known.  "r+"/"w+" must BE
 *   at the end: the next byte is peeked, and a stream that still has data
 *   refuses the write with EOPNOTSUPP - overwriting in the middle is the
 *   UPDAT half of #189.  A PDS member cannot be extended, so a member
 *   that has switched to reading only reads from then on.
 *
 * Returns 0, or -1 with errno set.  A stream whose reopen fails is left
 * open the way it was where that is possible; otherwise it is marked in
 * error, since there is no DCB left to use.
 *
 * The last, short block a writing DCB holds goes out at its CLOSE, not at
 * the flush.  When that write fails (#228) the switch still completes -
 * the data set is valid, only shorter - but the stream is marked in error
 * (ferror(), _FILE_FLAG_ENOSPC for an out-of-space) and the call answers
 * -1 with errno ENOSPC or EIO, so a reader cannot take the lost tail for
 * the end of the data.
 */
int
__fpswt(FILE *fp, int out)
{
    long    pos = fp->filepos;
    int     had;
    int     c;
    int     e;
    int     r;

    if (!(fp->flags & _FILE_FLAG_OPEN)) {
        errno = EBADF;
        return -1;
    }

    if (out) {
        if (fp->member[0] > ' ') {
            errno = EOPNOTSUPP;
            return -1;
        }

        if ((fp->flags & _FILE_FLAG_APPEND) && !(fp->flags & _FILE_FLAG_EOF)) {
            /* "a+" writes at the end whatever its read position: read on
               to it, so that the position stays known */
            while (__fgetc(fp) != EOF) ;
            if (fp->flags & _FILE_FLAG_ERROR) return -1;
            pos = fp->filepos;
        }
        else if (!(fp->flags & _FILE_FLAG_APPEND) && !(fp->flags & _FILE_FLAG_EOF)) {
            /* "r+"/"w+": is there anything after the position? */
            had = (fp->ungetch != -1);
            c = __fgetc(fp);
            if (fp->flags & _FILE_FLAG_ERROR) return -1;
            if (c != EOF) {
                /* put it back where it came from */
                if (had) {
                    fp->ungetch = c;
                }
                else {
                    fp->upto--;
                    fp->filepos--;
                }
                errno = EOPNOTSUPP;
                return -1;
            }
            pos = fp->filepos;
        }

    }

    /* pending output, or a record overwritten in place, goes first */
    if (__fflush(fp)) return -1;

    r = redcb(fp, out);
    if (r > 0) {
        /* reopened, but the old DCB's last block is lost: finish the
           switch, then report it (#228).  Only a writing DCB holds a
           block, so this is the turn to reading; every caller of
           __fpswt(fp, 1) has a reading DCB, the out arm is only there
           to leave the position right if that ever changes */
        e = errno;
        if (out) fp->filepos = pos;
        else     skipto(fp, pos);
        fp->flags |= _FILE_FLAG_ERROR;
        if (e == ENOSPC) fp->flags |= _FILE_FLAG_ENOSPC;
        errno = e;
        return -1;
    }
    if (r) {
        e = errno ? errno : EIO;
        /* a failed switch to writing: keep the stream readable */
        if (out && redcb(fp, 0) == 0) {
            fp->flags &= ~_FILE_FLAG_POSEND;
            skipto(fp, pos);
        }
        if (!(fp->flags & _FILE_FLAG_OPEN)) fp->flags |= _FILE_FLAG_ERROR;
        errno = e;
        return -1;
    }

    if (out) {
        fp->filepos = pos;
        return 0;
    }

    skipto(fp, pos);
    return 0;
}

/* close the DCB and open the same DD again, reading or writing.
   0 done, -1 the reopen failed (errno set), 1 reopened but the CLOSE
   could not write the last block (errno ENOSPC or EIO, #228) */
__asm__("\n&FUNC    SETC 'redcb'");
static int
redcb(FILE *fp, int out)
{
    int     lost = 0;
    int     rc;

    if (fp->flags & _FILE_FLAG_OPEN) {
        rc = __aclose(fp->dcb);
        if (rc) lost = (rc == 12) ? ENOSPC : EIO;
    }
    fp->dcb     = 0;
    fp->asmbuf  = 0;
    if (fp->buf) free(fp->buf);
    fp->buf     = 0;
    fp->upto    = 0;
    fp->endbuf  = 0;
    fp->ungetch = -1;
    fp->flags  &= ~(_FILE_FLAG_OPEN | _FILE_FLAG_DCBOUT | _FILE_FLAG_EOF);
    if (out) fp->flags |= _FILE_FLAG_DCBOUT;
    errno = 0;
    if (__fpopen(fp)) return -1;
    if (lost) {
        errno = lost;
        return 1;
    }
    return 0;
}

/* read forward from the start to pos - or to the end for POSEND */
__asm__("\n&FUNC    SETC 'skipto'");
static void
skipto(FILE *fp, long pos)
{
    fp->filepos = 0;
    if (fp->flags & _FILE_FLAG_POSEND) {
        while (__fgetc(fp) != EOF) ;
        fp->flags &= ~_FILE_FLAG_POSEND;
        return;
    }
    while (fp->filepos < pos) {
        if (__fgetc(fp) == EOF) break;
    }
}

/* __fpupc() - write c over the byte at the position (#189, slice 2).
 *
 * For a '+' stream whose reading DCB is open UPDAT (_FILE_XFLAG_UPDAT):
 * the record that holds the position is in fp->buf; c replaces a byte of
 * it, and the record goes back to the disk, same length, when the next
 * record is read, the stream turns round or closes (_FILE_XFLAG_DIRTY,
 * @@fflush.c updrec()).  A record never grows and never moves:
 *
 *   - text, '\n' where the record's own '\n' is: nothing to change, the
 *     position moves on to the next record;
 *   - text, '\n' inside an F record: the rest of the record becomes
 *     blanks and the position moves to the next record - the byte view,
 *     so LINEOUT f,"Done 3",3 over "Line 3" works;
 *   - text, '\n' inside a V or U record: it would shorten the record -
 *     EOPNOTSUPP;
 *   - any other byte where the record's '\n' is: it would lengthen the
 *     record - EOPNOTSUPP.  Bytes before it in the same write stay
 *     written.
 *
 * At the end of the data set there is nothing to overwrite: the stream
 * turns round to EXTEND, as in slice 1, and the caller writes normally.
 *
 * Returns 0 when c was written here, 1 when the DCB now writes at the end
 * and the caller carries on, -1 with errno set on refusal or error.
 */
int
__fpupc(FILE *fp, int c)
{
    unsigned char   *last;
    int             k;
    int             text = !(fp->flags & _FILE_FLAG_BINARY);

    fp->ungetch = -1;

    if (fp->upto >= fp->endbuf) {
        /* at a record boundary: bring in the record to overwrite */
        k = __fgetc(fp);
        if (fp->flags & _FILE_FLAG_ERROR) return -1;
        if (k == EOF) {
            /* the end: append */
            if (__fpswt(fp, 1)) return -1;
            return 1;
        }
        fp->upto--;
        fp->filepos--;
    }

    /* where a text record's '\n' is */
    last = text ? fp->endbuf - 1 : fp->endbuf;

    if (text && fp->upto >= last) {
        if (c == '\n') {
            fp->upto++;
            fp->filepos++;
            return 0;
        }
        errno = EOPNOTSUPP;
        return -1;
    }

    if (text && c == '\n') {
        if ((fp->recfm & _FILE_RECFM_TYPE) != _FILE_RECFM_F) {
            errno = EOPNOTSUPP;
            return -1;
        }
        memset(fp->upto, ' ', (size_t)(last - fp->upto));
        fp->filepos += (unsigned)(fp->endbuf - fp->upto);
        fp->upto     = fp->endbuf;
        fp->xflags  |= _FILE_XFLAG_DIRTY;
        return 0;
    }

    *fp->upto++ = (unsigned char)c;
    fp->filepos++;
    fp->xflags |= _FILE_XFLAG_DIRTY;
    return 0;
}
