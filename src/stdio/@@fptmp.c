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

/* option() - the value of name=v or name(v) in mode, copied into buf (n
** bytes), else the environment variable pfx followed by var, else NULL */
__asm__("\n&FUNC    SETC 'option'");
static const char *
option(const char *mode, const char *name, char *buf, size_t n,
       const char *pfx, const char *var)
{
    char        key[24];
    size_t      k       = strlen(name);
    const char  *p;
    char        *v;
    char        *tok_save = NULL;

    memcpy(key, name, k);
    key[k] = '=';
    key[k + 1] = 0;
    p = strstr(mode, key);
    key[k] = '(';
    if (!p) p = strstr(mode, key);
    if (p) {
        strncpy(buf, p + k + 1, n - 1);
        buf[n - 1] = 0;
        v = __tok(buf, " ,)", &tok_save);
        if (v) return v;
    }
    strcpy(key, pfx);
    strcat(key, var);
    return getenv(key);
}

/* __fpdcb() - the RECFM, LRECL and, for a blocked format, BLKSIZE text
** units of a data set that fopen() allocates: from the mode string, else
** from the environment variables pfx RECFM, LRECL and BLKSIZE ("TEMP_",
** "SYSOUT_"), else V and 255 (#474).  The mode string is in lower case,
** the environment in upper.  Here, because every program that can reach
** __fpstar() links this module too. */
int
__fpdcb(TXT99 ***txt99, const char *mode, const char *pfx)
{
    char        buf[16];
    const char  *v;
    int         err;
    int         blocked;

    v = option(mode, "recfm", buf, sizeof(buf), pfx, "RECFM");
    if (!v) v = "V";
    err = __txrecf(txt99, v);
    if (err) return err;
    blocked = strchr(v, 'B') || strchr(v, 'b');

    v = option(mode, "lrecl", buf, sizeof(buf), pfx, "LRECL");
    if (!v) v = "255";
    err = __txlrec(txt99, v);
    if (err || !blocked) return err;

    v = option(mode, "blksize", buf, sizeof(buf), pfx, "BLKSIZE");
    return v ? __txbksz(txt99, v) : 0;
}

int
__fptmp(FILE *fp)
{
    int         err     = 1;
    unsigned    count   = 0;
    char        *space  = 0;
    char        *p      = 0;
    TXT99       **txt99 = NULL;
    RB99        rb99    = {0};
    char        temp[sizeof(fp->mode)+1];

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

    /* RECFM, LRECL, BLKSIZE */
    err = __fpdcb(&txt99, fp->mode, "TEMP_");
    if (err) goto quit;

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
