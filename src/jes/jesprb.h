/* JESPRB.H - JES2 spool block parser: the asm-free half of jesprint().
 *
 * Self-contained on purpose (#25).  It includes no libc370 header, so a host
 * `cc` can compile and link jesprb.c and exercise the REAL record walk
 * (test/host/tstjesprb.c) instead of a hand-written mirror.  clibjes2.h is not
 * usable here: it pulls libc370's time.h/time64.h, which collide with the host
 * ones.
 *
 * Everything that needs MVS stays on the caller's side of the emit callback:
 * the BDAM spool_read(), the EX/TR translate of unprintable characters, the
 * jobkey/dsid checks and the block chain itself are all in jesprint.c.
 */
#ifndef JESPRB_H
#define JESPRB_H

/* One print record.  The record that follows starts at
 * data + (flags & FLAG_HASCC ? 1 : 0) + len.                                */
typedef struct prline {
    unsigned char   len;        /* length of data (not including cc if any) */
#define EOB         0xff        /* indicates end of block */

    unsigned char   flags;      /* processing flags for data */
#define FLAG_HASCC  0x80        /* ... first data byte is carriage control char */
#define FLAG_SPAN   0x10        /* ... spanned line/record */
#define FLAG_FIRST  0x08        /* ... first part of spanned line */
#define FLAG_MIDDLE 0x04        /* ... middle part of spanned line */
#define FLAG_LAST   0x02        /* ... last part of spanned line */

    unsigned char   len2;       /* looks like len but not always the same??? */
    unsigned char   data[0];    /* start of print line (including cc if any) */
} PRLINE;

/* A record that is (part of) a line spanning several records or blocks.  The
 * FIRST part carries a 2-byte total length in front of the data.            */
typedef struct spline {
    unsigned char   len;        /* length of data (may be zero on spanned lines) */
    unsigned char   flags;      /* same as prline.flags */
    unsigned short  len2;       /* length of this line part */
    unsigned char   data[0];    /* start of spanned print line */
} SPLINE;

/* Block header; the first record starts at offset 10.                       */
typedef struct prblock {
    unsigned int    next;
    unsigned int    jobkey;
    unsigned short  dsid;
} PRBLOCK;

/* Parser state and outcome.  It lives across the blocks of one data set
 * because a spanned line legitimately continues into the next block, so the
 * caller keeps one of these for the whole walk and zeroes it before the first
 * block.  The caller also frees prbuf when the walk is over.                */
typedef struct jesprb {
    char       *prbuf;          /* reassembly buffer for a spanned line      */
    unsigned    blksize;        /* size of prbuf: the LARGEST total announced
                                   so far, since the buffer only ever grows  */
    unsigned    total;          /* what the FIRST part of the line now being
                                   assembled announced.  Not the same as
                                   blksize - a short line after a long one
                                   would otherwise be measured against the
                                   long one's buffer and could overrun its
                                   own announcement unnoticed (#24)          */
    unsigned    linelen;        /* bytes of the spanned line assembled so far*/
    unsigned    lines;          /* lines handed to emit(), all blocks so far */
    int         reason;         /* JESPRB_* for the block just walked        */
    int         prtrc;          /* emit() rc when reason == JESPRB_STOPPED   */
    int         assembling;     /* a FIRST part opened a line and no LAST has
                                   closed it: only then may a MIDDLE/LAST be
                                   appended, and only then is prbuf sized for
                                   the line now arriving (#24)              */
} JESPRB;

/* Why the record walk of ONE block ended.  jesprint() maps these onto the
 * public JESPR_* reasons in clibjes2.h; they are deliberately separate so
 * this translation unit stays free of libc370 headers.                      */
#define JESPRB_OK       0       /* end of block, keep following the chain    */
#define JESPRB_STOPPED  1       /* emit() asked to stop (its rc is in prtrc) */
#define JESPRB_NOBUF    2       /* a spanned part that cannot be reassembled:
                                   a MIDDLE/LAST with no FIRST opening the
                                   line, or parts adding up to more than the
                                   FIRST part announced                      */
#define JESPRB_NOMEM    3       /* reassembly buffer could not be allocated  */
#define JESPRB_TRUNC    4       /* a record runs past the end of the block:
                                   the block is truncated or malformed       */

/* Walk the records of one spool block and hand each line to emit().
 *
 * blk/blklen is the block as read from the spool (blklen = the JES2 BUFSIZE),
 * pb the state carried across blocks.  emit() gets a pointer INTO blk (or into
 * the reassembly buffer for a spanned line) and may modify it in place - the
 * MVS caller translates unprintable characters there.  A negative emit() rc
 * stops the walk.
 *
 * Returns 0 when the caller should follow the chain to the next block (that
 * includes JESPRB_NOBUF and JESPRB_TRUNC, which only end the record walk of
 * this block), and a negative value when the whole walk must stop;
 * pb->reason says why.
 *
 * Giving up on a block hands out whatever of a spanned line was already
 * assembled - a visible fragment beats a line that silently disappears - and
 * then drops the reassembly state, so the next block cannot append to a line
 * whose middle this block could not read (#24).                            */
int __jesprb(char *blk, unsigned blklen, JESPRB *pb,
             int (*emit)(char *line, unsigned linelen, void *arg), void *arg);

#endif /* JESPRB_H */
