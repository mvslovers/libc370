/* JESPRINT.C - Print JES Job by DSID */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hasphct.h"    /* JES Checkpoint Control Table, record 3 in HASPCKPT */
#include "haspjct.h"    /* JES Job Control Table                            */
#include "hasppddb.h"   /* JES PDDB Print Datasets                          */
#include "haspiot.h"    /* JES IOT                                          */
#include "ieftxtft.h"   /* text string types                                */
#include "iefvkeys.h"   /* text key values                                  */
#include "clibjes2.h"   /* JES prototypes */
#include "clibary.h"    /* dynamic array */

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

typedef struct spline {
    unsigned char   len;        /* length of data (may be zero on spanned lines) */
    unsigned char   flags;      /* same as prline.flags */
    unsigned short  len2;       /* length of this line part */
    unsigned char   data[0];    /* start of spanned print line */
} SPLINE;

typedef struct prblock {
    unsigned int    next;
    unsigned int    jobkey;
    unsigned short  dsid;
} PRBLOCK;

static int esc_print(int(*prt)(const char *, unsigned, void *), char *line,
                     unsigned linelen, void *arg);

int jesprint(JES *jes, JESJOB *job, unsigned dsid,
             int(*prt)(const char *line, unsigned linelen, void *arg),
             void *arg, JESPRST *st)
{
    int         rc      = 503;      /* service unavailable */
    JESPRST     stat;               /* used when the caller passes st = NULL */
    JESDD       *jesdd  = NULL;
    HASPCP      *cp     = NULL;
    HASPJS      *js     = NULL;
    __HCT       *hct    = NULL;
    char        *buf    = NULL;
    char        *prbuf  = NULL;
    char        *p;
    char        *eob;
    PRBLOCK     *block;
    PRLINE      *line;
    unsigned    linelen;
    unsigned    blksize = 0;
    unsigned    bufsize;
    unsigned    mttr;
    unsigned    count;
    unsigned    n;
    int         i;

    if (!st) st = &stat;            /* one code path, no NULL tests below    */
    memset(st, 0, sizeof(JESPRST));
    st->reason = JESPR_EMPTY;       /* nothing walked yet                    */

    if (!jes) goto quit;
    if (!job) goto quit;
    if (!dsid) goto quit;

    cp     = jes->cp;
    if (!cp) goto quit;

    hct    = &cp->hct;
    bufsize = hct->_BUFSIZE;

    js     = jes->js[0];
    if (!js) goto quit;

    buf = calloc(1, bufsize);
    if (!buf) {
        wtof("Unable to allocate storage for %u byte buffer", bufsize);
        goto quit;
    }
    eob = &buf[bufsize-sizeof(PRLINE)];

    count = arraycount(&job->jesdd);
    for(n=0; n < count; n++) {
        JESDD *dd = job->jesdd[n];

        if (!dd) continue;
        /* wtof("dd->dsid=%u, dsid=%u", dd->dsid, dsid); */
        if (dd->dsid == dsid) {
            jesdd = dd;
            break;
        }
    }

    rc = 404;   /* not found */
    if (!jesdd) goto quit;

    /* at this point we know which DSID in the job we want to start with */
    rc = 0;
    if (!jesdd->mttr) goto quit;    /* no sysout for this dsid, nothing to do */

    /* process the sysout dataset */
    st->reason = JESPR_END;         /* until something stops the walk early  */

    for(block = (PRBLOCK*)buf, mttr = jesdd->mttr; mttr; mttr = block->next) {
        /* runaway guard: the next address comes out of the block we read */
        if (st->blocks >= JESPR_MAXBLK) {
            st->reason = JESPR_CAP;
            st->mttr   = mttr;
            break;
        }

        /* read block from spool dataset */
        if (spool_read(js, mttr, buf, hct->_BUFSIZE)) {
            st->reason = JESPR_IOERR;
            st->mttr   = mttr;
            break;
        }

        /* make sure this block is for our job.  A mismatch is how a purged
           data set announces itself: the checkpointed PDDB still points at
           tracks that now belong to somebody else.                         */
        if (job->jobkey != block->jobkey) {
            st->reason = JESPR_FOREIGN;
            st->mttr   = mttr;
            break;
        }
        if (jesdd->dsid != block->dsid) {
            st->reason = JESPR_DSID;
            st->mttr   = mttr;
            break;
        }

        st->blocks++;
#if 0
        wtodumpf(buf, sizeof(PRBLOCK), "PRBLOCK");
#endif
        /* unwind the print lines in the block */
        for(p=&buf[10], line=(PRLINE*)p; line->len != EOB && p < eob; line=(PRLINE*)p) {
#if 0
            wtodumpf(line, line->len + sizeof(PRLINE), "PRLINE");
            if (line->len==0 && line->flags & FLAG_SPAN) {
                wtodumpf(buf, bufsize, "SPANNED LINE BLOCK");
                break;
            }
#endif
            if (line->flags & FLAG_SPAN) {
                SPLINE *sp = (SPLINE*)line;

                /* wtodumpf(sp, sizeof(SPLINE), "SPLINE"); */

                p = sp->data;
                if (sp->flags & FLAG_FIRST) {
                    unsigned newblock = *(unsigned short *)p;
                    p += 2;

                    if (newblock > blksize) {
                        if (prbuf) free(prbuf);
                        blksize = newblock;
                        prbuf = calloc(1, blksize + 4);
                        if (!prbuf) {
                            wtof("Unable to allocate storage for %u byte buffer", blksize + 4);
                            goto quit;
                        }
                    }

                    linelen = 0;
                    if (sp->flags & FLAG_HASCC) p++;
                }

                if (!prbuf) break;  /* we don't have a print buffer for the spanned record */

                memcpy(&prbuf[linelen], p, sp->len2);
                linelen += sp->len2;

                if (sp->flags & FLAG_LAST || linelen > blksize) {
                    if (linelen) st->lines++;
                    rc = esc_print(prt, prbuf, linelen, arg);
                    if (rc < 0) {
                        st->reason = JESPR_STOPPED;
                        st->prtrc  = rc;
                        st->mttr   = mttr;
                        goto quit;
                    }
                    linelen = 0;
                }

                p+=sp->len2;
                continue;
            }

            p = line->data;
            if (line->flags & FLAG_HASCC) p++;  /* skip over carriage control character */

            /* print this line after HTML escaping it */
            if (line->len) st->lines++;
            rc = esc_print(prt, p, line->len, arg);
            if (rc < 0) {
                st->reason = JESPR_STOPPED;
                st->prtrc  = rc;
                st->mttr   = mttr;
                goto quit;
            }
            p+=line->len;
        }

        /* a block that chains to itself would spin forever */
        if (block->next == mttr) {
            st->reason = JESPR_LOOP;
            st->mttr   = mttr;
            break;
        }
    }

quit:
    if (prbuf) free(prbuf);
    if (buf) free(buf);
    return rc;
}

__asm__("\n&FUNC    SETC 'esc_print'");
static int
esc_print(int(*prt)(const char *, unsigned, void *), char *line,
          unsigned linelen, void *arg)
{
    int         rc = 0;
    char        *p;
    char        *e;
    unsigned    n;
    char        buf[1024];

    if (!linelen) goto quit;

    /* wtodumpf(line, linelen, "esc_print()"); */

    /* here we attempt to remove any trailing spaces */
    while(linelen > 1 && line[linelen-1]==' ') linelen--;

    /* here we limit linelen to 255 (mostly for the inline translate of unprintable characters) */
    if (linelen > 255) linelen = 255;

    /* translate any unprintable characters to spaces */
    __asm("LR\t1,%0          => our print line\n\t"
          "LR\t14,%1         => length of line\n\t"
          "BCTR\t14,0        decrement for execute\n\t"
          "EX\t14,TRLINE     translate unsafe characters"
          : :"r"(line), "r"(linelen) : "14", "1");

#if 0 /* we don't really need this since we're sending out "text/plain" lines */
    /* escape HTML special characters */
    for(p=buf, e=&buf[sizeof(buf)-1], n=0; (n < linelen) && (p < e); n++) {
        switch(line[n]) {
        case '&':
            *p++ = '&';
            *p++ = 'a';
            *p++ = 'm';
            *p++ = 'p';
            *p++ = ';';
            break;
        case '<':
            *p++ = '&';
            *p++ = 'l';
            *p++ = 't';
            *p++ = ';';
            break;
        case '>':
            *p++ = '&';
            *p++ = 'g';
            *p++ = 't';
            *p++ = ';';
            break;
        default:
            *p++ = line[n];
            break;
        }
    }

    linelen = (unsigned) (p - buf);

    /* print the translated and escaped buffer */
    rc = prt(buf, linelen, arg);
#else
    /* print the translated buffer */
    rc = prt(line, linelen, arg);
#endif
quit:
    return rc;
}
__asm("\nTRLINE   TR    0(*-*,1),PRTXLATE   REMOVE UNPRINTABLES");
__asm("\nPRTXLATE DC    64C' ',191AL1(*-PRTXLATE),C' '");

