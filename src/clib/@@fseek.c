/* @@FSEEK.C - caller should hold lock on file handle */
#include <stdio.h>
#include <errno.h>

int
__fseek(FILE *fp, long int offset, int whence)
{
    long oldpos;
    long newpos;
    char fnm[FILENAME_MAX];
    long int x;
    size_t y;
    char buf[1000];

    /* *any* seek will clear the error and eof indicators.  C says fseek
       clears eof only and rewind clears both; that deviation is its own
       issue and is not touched here.  _FILE_FLAG_ENOSPC must follow
       _FILE_FLAG_ERROR wherever it is cleared, or it dangles (#149). */
    fp->flags   &= 0xFFFF - _FILE_FLAG_ERROR - _FILE_FLAG_ENOSPC;
    fp->flags   &= 0xFFFF - _FILE_FLAG_EOF;

    /* get current offset in this stream */
    oldpos = fp->filepos;
    if (fp->flags & _FILE_FLAG_WRITE) {
        __fflush(fp);
    }

    if (whence == SEEK_SET) {
        newpos = offset;
    }
    else if (whence == SEEK_CUR) {
        newpos = oldpos + offset;
    }
    else if (whence == SEEK_END && !(fp->flags & _FILE_FLAG_READ)) {
        /* a write-only stream is always at its end (#200) */
        newpos = oldpos + offset;
    }
    else if (whence == SEEK_END) {
        /* read until end of file or error */
        while (__fread(buf, sizeof(buf), 1, fp) == 1) {
            /* do nothing */
        }
        goto quit;
    }
    else {
        /* not a whence we know -- there is no position to seek to, and
        ** every use of newpos below would be reading an unset local. */
        return (-1);
    }

    /* A stream not open for reading cannot move (#200).  Every way of
       getting somewhere else goes through a read - forward by __fgetc(),
       backward by a reopen, which for "w" truncates the data set - and a
       read on an output DCB served the stale write buffer instead
       (fseek(3) after "ABCDEF" wrote "ABCxy" as a second record).  So it
       may only "seek" to where it already is, which is also its end.
       Anything else fails without touching the stream: no error
       indicator, because that would refuse every later write (#149).
       w+/r+/a+ (#189) is what will make a writer movable. */
    if (!(fp->flags & _FILE_FLAG_READ)) {
        if (newpos == oldpos) goto quit;
        errno = ESPIPE;
        return (-1);
    }

    /* if we're in read mode */
    if (fp->flags & _FILE_FLAG_READ) {
        if (fp->flags & _FILE_FLAG_RECORD) {
            /* record mode, newpos is desired record number */
            if (newpos == fp->filepos) {
                fp->upto = fp->buf;
                goto quit;
            }
        }
        else {
            /* calculate RBA for start and end of buffer */
            long start = oldpos - (fp->upto - fp->buf);
            long end   = oldpos + (fp->endbuf - fp->upto);

            /* is newpos within the current buffer? */
            if ((newpos >= start) && (newpos < end)) {
                /* yes, reposition and we're done */
                fp->upto = fp->buf + (size_t)(newpos - oldpos);
                goto quit;
            }
        }
    }

    if (newpos < oldpos) {
        /* we need to reopen the file */
        if (fp->member[0] > ' ') {
            sprintf(fnm, "dd:%s(%s)", fp->ddname, fp->member);
        }
        else {
            sprintf(fnm, "dd:%s", fp->ddname);
        }
        if (__reopen(fnm, fp->mode, fp) == NULL) goto quit;
        oldpos = 0;
    }

    if (fp->flags & _FILE_FLAG_RECORD) {
        /* record mode, newpos is desired record number */
        for(x=oldpos; x < newpos; x++) {
            __fread(buf, sizeof(buf), 1, fp);
        }
        goto quit;
    }
    else {
        /* byte stream mode, newpos is desired byte offset in file */
        int c;
        for(x=oldpos; x < newpos; x++) {
            c = __fgetc(fp);
            if (c==EOF) {
                fp->flags |= (_FILE_FLAG_ERROR + _FILE_FLAG_EOF);
                goto quit;
            }
        }
    }

quit:
    if (fp->flags & _FILE_FLAG_ERROR) {
        /* well that's not good, something went wrong */
        return (-1);
    }

    /* success */
    fp->ungetch = -1;   /* discard the unget character  */
    return (0);
}
