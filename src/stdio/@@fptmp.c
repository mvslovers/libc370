/* @@FPTMP.C */
#include "src/internal/fileio.h"
#include "src/internal/tok.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <stddef.h>
#include <mvs/dynalloc.h>
#include "mvs/dynalloc.h"
#include "ext/array.h"

/* SPACE in blocks: the length is the text from s up to end */
__asm__("\n&FUNC    SETC 'txblk'");
static int
txblk(TXT99 ***txt99, const char *s, const char *end)
{
    char    blk[12];
    size_t  n = (size_t)(end - s);

    if (n >= sizeof(blk)) n = sizeof(blk) - 1;
    memcpy(blk, s, n);
    blk[n] = 0;
    return __txblk(txt99, blk);
}

/* __fpbks() - the BLKSIZE text unit for a data set that fopen() allocates,
** from blksize=n or blksize(n) in mode, else from the environment variable
** env; none when neither is there.  Here, because every program that can
** reach __fpstar() links this module too (#474). */
int
__fpbks(TXT99 ***txt99, const char *mode, const char *env)
{
    const char  *p;
    char        *blksize    = 0;
    char        *tok_save   = NULL;
    char        temp[16];

    p = strstr(mode, "blksize=");
    if (!p) p = strstr(mode, "blksize(");
    if (p) {
        strncpy(temp, p + 8, sizeof(temp) - 1);
        temp[sizeof(temp) - 1] = 0;
        blksize = __tok(temp, " ,)", &tok_save);
    }
    if (!blksize) blksize = getenv(env);
    return blksize ? __txbksz(txt99, blksize) : 0;
}

int
__fptmp(FILE *fp)
{
    int         err     = 1;
    unsigned    count   = 0;
    char        *recfm  = 0;
    char        *lrecl  = 0;
    char        *space  = 0;
    int         blocked;
    char        *p      = 0;
    TXT99       **txt99 = NULL;
    RB99        rb99    = {0};
    char        temp[sizeof(fp->mode)+1];
    char        *tok_save = NULL;

    /* we want the DDNAME returned to us */
    err = __txrddn(&txt99, NULL);
    if (err) goto quit;

    /* allocate this dataset */
    err = __txdsn(&txt99, fp->dataset);
    if (err) goto quit;

    /* DISP=NEW */
    err = __txnew(&txt99, NULL);
    if (err) goto quit;

    /* DISP=,CATALOG */
    err = __txcat(&txt99, NULL);
    if (err) goto quit;

    /* DSORG=PS */
    err = __txorg(&txt99, "PS");
    if (err) goto quit;

    /* get record format */
    p = strstr(fp->mode, "recfm=");
    if (!p) p = strstr(fp->mode,"recfm(");
    if (p) {
        strcpy(temp,p+6);
        recfm = __tok(temp, " ,)", &tok_save);
    }
    if (!recfm) recfm = getenv("TEMP_RECFM");
    if (!recfm) recfm = "V";
    err = __txrecf(&txt99, recfm);
    if (err) goto quit;
    /* now: recfm points into temp, which the options below reuse.  The
       mode string is in lower case, the environment in upper (#474) */
    blocked = strchr(recfm, 'B') || strchr(recfm, 'b');

    /* get logical record length */
    p = strstr(fp->mode, "lrecl=");
    if (!p) p = strstr(fp->mode,"lrecl(");
    if (p) {
        strcpy(temp,p+6);
        lrecl = __tok(temp, " ,)", &tok_save);
    }
    if (!lrecl) lrecl = getenv("TEMP_LRECL");
    if (!lrecl) lrecl = "255";
    err = __txlrec(&txt99, lrecl);
    if (err) goto quit;

    if (blocked) {
        /* get optional block size */
        err = __fpbks(&txt99, fp->mode, "TEMP_BLKSIZE");
        if (err) goto quit;
    }

    /* get optional space "[CYL|TRK](pri[,sec])" or "blksize(pri[,sec])" */
    p = strstr(fp->mode, "space=");
    if (p) {
        strcpy(temp,p+6);
        p = strchr(temp, '(');
        if (p) {
            p = strchr(p, ')');
            if (p) {
                space = temp;
                p++;    /* skip over the ')' */
                *p=0;   /* terminate the string here */
            }
        }
    }
    if (!space) space = getenv("TEMP_SPACE");
    if (space) {
        char *x = strchr(space,'(');
        if (x) {
            /* "cyl" from the mode string, "CYL" from TEMP_SPACE (#474) */
            if (strncasecmp(space, "cyl", 3)==0) {
                /* CYLINDERS */
                err = __txcyl(&txt99, NULL);
            }
            else if (strncasecmp(space, "trk", 3)==0) {
                /* TRACKS */
                err = __txtrk(&txt99, NULL);
            }
            else {
                /* BLOCKS of the length before the '(' */
                err = txblk(&txt99, space, x);
            }
            if (err) goto quit;
            space = x+1;
        }
        while(*space && !isdigit(*space)) space++;
        err = __txspac(&txt99, space);  /* "pri[,sec]" */
        if (err) goto quit;
    }

    /* we want temp dataset allocated to VIO */
    err = __txunit(&txt99, "VIO");
    if (err) goto quit;

    count = arraycount(&txt99);
    if (!count) goto quit;

    /* Set high order bit to mark end of list */
    count--;
    txt99[count]    = (TXT99*)((unsigned)txt99[count] | 0x80000000);

    /* construct the request block for dynamic allocation */
    rb99.len        = sizeof(RB99);
    rb99.request    = S99VRBAL;
    rb99.flag1      = S99NOCNV;
    rb99.txtptr     = txt99;

    /* SVC 99 */
    err = __svc99(&rb99);
    if (err) goto quit;

    /* return DDNAME */
    memcpy(fp->ddname, txt99[0]->text, 8);
    fp->flags |= _FILE_FLAG_DYNAMIC;

quit:
    if (txt99) FreeTXT99Array(&txt99);

    return err;
}
