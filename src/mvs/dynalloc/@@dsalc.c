/* @@DSALC.C */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stddef.h>
#include <mvs/dynalloc.h>
#include "mvs/dynalloc.h"
#include "ext/array.h"
#include "mvs/crt.h"
#include "stdio.h"

typedef struct __dsalc    __DSALC;
struct __dsalc {
    char		*blksize;   // blksize=...
    char        *ddn;       // ddname=...
    char        *disp;      // disp=(new,catlg,delete)
    char        *dcbdsn;    // dcbdsn=... (DCB model reference)
    char        *dsn;       // dsname=...
    char        *dsorg;     // dsorg=PS or PO or ...
    char        *lrecl;     // lrecl=...
    char        *recfm;     // recfm=...
    char        *space;     // space=trk(n,n,n) or cyl(n,n,n) 
    char        *unit;      // unit=...
    char        *volser;    // volser=...
    int         mount;      // mount - let SVC 99 ask for a mount (#181)
    int         freeclose;  // free=close - unallocate at CLOSE (#408)
};

/* p is at a '(' in temp: "TRK(n,n)" becomes "TRK=n,n" and "name=(a,b)"
   becomes "name=a,b".  The shifts overlap, so memmove(), not strcpy()
   (#171), and a '(' first in temp has nothing before it to read.  Returns
   where the ')' was, or the end of the string. */
static char *
unparen(char *temp, char *p)
{
    if (p == temp || *(p-1) != '=') {
        *p = '=';
    }
    else {
        memmove(p, p+1, strlen(p+1)+1);
    }

    for(p++;*p; p++) {
        if (*p==')') {
            memmove(p, p+1, strlen(p+1)+1);
            break;
        }
    }
    return p;
}

/* tok is the keyword being converted.  The parentheses of a data set name
   are its member (or relative generation), which __txdsn() turns into its
   own text unit: they stay.  unparen() made "DSN=A.B(MEM)" "DSN=A.B=MEM"
   (#408). */
static int
isdsn(const char *tok)
{
    while (*tok == ' ') tok++;
    return strncmp(tok, "DSN=", 4) == 0 || strncmp(tok, "DSNAME=", 7) == 0;
}

/* the SPACE text units from space as __dsalc() leaves it: the unit (TRK,
   CYL or a block length) up to the first '=' or ',', then the quantities,
   the parentheses around them dropped */
__asm__("\n&FUNC    SETC 'dsspace'");
static int
dsspace(TXT99 ***txt99, const char *space)
{
    const char  *x      = strpbrk(space, "=,");
    char        unit[12];
    char        qty[32];
    size_t      n       = x ? (size_t)(x - space) : strlen(space);
    int         err;

    if (n >= sizeof(unit)) n = sizeof(unit) - 1;
    memcpy(unit, space, n);
    unit[n] = 0;
    if (strcmp(unit, "CYL") == 0) err = __txcyl(txt99, NULL);
    else if (strcmp(unit, "TRK") == 0) err = __txtrk(txt99, NULL);
    else err = __txblk(txt99, unit);
    if (err || !x) return err;

    for (x++; *x && !isdigit((unsigned char)*x); x++) ;
    n = strcspn(x, ")");
    if (n >= sizeof(qty)) n = sizeof(qty) - 1;
    memcpy(qty, x, n);
    qty[n] = 0;
    return __txspac(txt99, qty);        /* "pri[,sec][,dir]" */
}

int
__dsalc(char *ddname, const char *opts)
{
    CLIBCRT     *crt        = __crtget();
    char        *crtstrtk;
    int         err         = 1;
    char        *temp       = NULL;
    int         templen     = 0;
    __DSALC     dsalcbuf    = {0};
    __DSALC     *dsalc      = &dsalcbuf;
    TXT99       **txt99     = NULL;
    RB99        rb99        = {0};
    unsigned    count;
    char        *p;
    char 		*t;

    if (!crt) return err;       /* no CRT: no strtok state (#85) */
    crtstrtk = crt->crtstrtk;

	// wtof("%s: enter", __func__);
	
    if (opts) {
        templen = strlen(opts) + 8;
        temp    = calloc(1, templen);
        if (!temp) goto quit;   /* shortage arrives as NULL; malloc() has already logged it */
        
        /* copy opts string to temp and upper case */
        for(p=temp, t=(char*)opts; *t; p++, t++) {
            *p = toupper(*t);
        }
            
        /* we want to convert our opts string "dd=test;space=TRK(val1,val2);..."
        * to something like  "DD=TEST;SPACE=TRK,val1,val2;..." 
        */
        // wtof("%s: opts=\"%s\"", __func__, opts);
        for(p=temp, t=temp; *p; p++) {

            if (*p=='(' && isdsn(t)) {
                p = strchr(p, ')');     /* "A.B(MEM)" goes on as it is */
                if (!p) break;
                continue;
            }
            if (*p=='(') p = unparen(temp, p);

            if (*p==',') *p = ';';
            if (*p==';') t = p + 1;     /* the next keyword starts */
        }
    }
    else {
        /* no opts: an empty string to parse.  With temp NULL, strtok(NULL)
           went on with the caller's own pending strtok() (#408) */
        temp = calloc(1, 1);
        if (!temp) goto quit;
    }
    
	// wtof("%s: temp=\"%s\"", __func__, temp);

	for(p=strtok(temp, ";"); p; p=strtok(NULL, ";")) {
        if (strcmp(p, "MOUNT")==0) dsalc->mount = 1;
        else if (strcmp(p, "FREE=CLOSE")==0) dsalc->freeclose = 1;
        else if (strstr(p, "BLKSIZE=")) dsalc->blksize = p+8;
        else if (strstr(p, "DD=")) dsalc->ddn = p+3;
        else if (strstr(p, "DDNAME=")) dsalc->ddn = p+7;
        else if (strstr(p, "DISP=")) dsalc->disp = p+5;
        /* DCBDSN= must be tested before DSN=: this dispatch uses strstr(),
        * not a prefix compare, and "DCBDSN=" contains "DSN=" at offset 3 --
        * so the DSN= branch would match a model reference and take the data
        * set name from four bytes into the middle of the keyword. */
        else if (strstr(p, "DCBDSN=")) dsalc->dcbdsn = p+7;
        else if (strstr(p, "DSN=")) dsalc->dsn = p+4;
        else if (strstr(p, "DSNAME=")) dsalc->dsn = p+7;
        else if (strstr(p, "DSORG=")) dsalc->dsorg = p+6;
		else if (strstr(p, "RECFM=")) dsalc->recfm = p+6;
		else if (strstr(p, "LRECL=")) dsalc->lrecl = p+6;
		else if (strstr(p, "SPACE=")) dsalc->space = p+6;
        else if (strstr(p, "UNIT=")) dsalc->unit = p+5;
        else if (strstr(p, "VOLSER=")) dsalc->volser = p+7;
	}

#if 0 /* debugging */
	wtof("%s: blksize   =\"%s\"", __func__, dsalc->blksize);
    wtof("%s: ddn       =\"%s\"", __func__, dsalc->ddn);
    wtof("%s: disp      =\"%s\"", __func__, dsalc->disp);
    wtof("%s: dcbdsn    =\"%s\"", __func__, dsalc->dcbdsn);
    wtof("%s: dsn       =\"%s\"", __func__, dsalc->dsn);
    wtof("%s: dsorg     =\"%s\"", __func__, dsalc->dsorg);
	wtof("%s: lrecl     =\"%s\"", __func__, dsalc->lrecl);
	wtof("%s: recfm     =\"%s\"", __func__, dsalc->recfm);
	wtof("%s: space     =\"%s\"", __func__, dsalc->space);
    wtof("%s: unit      =\"%s\"", __func__, dsalc->unit);
    wtof("%s: volser    =\"%s\"", __func__, dsalc->volser);
#endif

    if (dsalc->ddn && *dsalc->ddn) {
        /* allocate to this DD name */
        err = __txddn(&txt99, dsalc->ddn);
    }
    else {
        /* we want the DDNAME returned to us */
        err = __txrddn(&txt99, NULL);
    }
    if (err) goto quit;

    if (dsalc->dsn && *dsalc->dsn) {
        /* allocate this dataset */
        err = __txdsn(&txt99, dsalc->dsn);
        if (err) goto quit;
    }

    if (dsalc->disp) {
        /* DISP=alloc[,normal[,abnormal]] */
        p = strtok(dsalc->disp, " ,");
        if (!p || *p==0) err = __txnew(&txt99, NULL);
        else if (strstr(p, "NEW")) err = __txnew(&txt99, NULL);
        else if (strstr(p, "OLD")) err = __txold(&txt99, NULL);
        else if (strstr(p, "MOD")) err = __txmod(&txt99, NULL);
        else if (strstr(p, "SHR")) err = __txshr(&txt99, NULL);
        else err = 1;           /* unknown token -- fail, do not allocate without it */
        if (err) goto quit;
        
        p = strtok(NULL, " ,");
        if (p) {
            /* normal disposition.  UNCAT before CAT: strstr() finds "CAT"
               in "UNCATLG", which cataloged what was to be uncataloged
               (#408) */
            if (strstr(p, "DEL")) err = __txdel(&txt99, NULL);
            else if (strstr(p, "UNCAT")) err = __txucat(&txt99, NULL);
            else if (strstr(p, "CAT")) err = __txcat(&txt99, NULL);
            else if (strstr(p, "KEEP")) err = __txkeep(&txt99, NULL);
            else err = 1;
            if (err) goto quit;
        }
        
        p = strtok(NULL, " ,");
        if (p) {
            /* abnormal disposition, the same order (#408) */
            if (strstr(p, "DEL")) err = __txadel(&txt99, NULL);
            else if (strstr(p, "UNCAT")) err = __txauca(&txt99, NULL);
            else if (strstr(p, "CAT")) err = __txacat(&txt99, NULL);
            else if (strstr(p, "KEEP")) err = __txakee(&txt99, NULL);
            else err = 1;
            if (err) goto quit;
        }
    }

    if (dsalc->dcbdsn) {
        /* DCBDSN=dsname -- JCL DCB=(dsname).  Emitted ahead of the explicit
        * DCB attributes below so those override the model, the way
        * DCB=(model,RECFM=FB) reads in JCL. */
        err = __txdcbd(&txt99, dsalc->dcbdsn);
        if (err) goto quit;
    }

    if (dsalc->dsorg) {
        /* DSORG=xx */
        err = __txorg(&txt99, dsalc->dsorg);
        if (err) goto quit;
    }

    if (dsalc->recfm) {
        /* RECFM=xx */
        err = __txrecf(&txt99, dsalc->recfm);
        if (err) goto quit;
    }
    
    if (dsalc->lrecl) {
        /* LRECL=xx */
        err = __txlrec(&txt99, dsalc->lrecl);
        if (err) goto quit;
    }

    if (dsalc->blksize) {
        /* BLKSIZE=xx */
        err = __txbksz(&txt99, dsalc->blksize);
        if (err) goto quit;
    }

    /* get optional space: "TRK(pri,sec)" arrives as "TRK=pri,sec", the
       JCL form "(TRK,(pri,sec))" as "TRK,(pri,sec)", and a block length
       as "800=pri,sec" or "800,(pri,sec)".  The JCL form fell into the
       blocks branch with a length of "TRK,(1,1)" (#408). */
    if (dsalc->space) {
        err = dsspace(&txt99, dsalc->space);
        if (err) goto quit;
    }

    if (dsalc->unit) {
        err = __txunit(&txt99, dsalc->unit);
        if (err) goto quit;
    }

    if (dsalc->volser) {
        err = __txvols(&txt99, dsalc->volser);
        if (err) goto quit;
    }

    if (dsalc->freeclose) {
        /* FREE=CLOSE: the DD goes when the data set is closed (#408) */
        err = __txclos(&txt99, NULL);
        if (err) goto quit;
    }


    count = arraycount(&txt99);
    if (!count) goto quit;

    /* Set high order bit to mark end of list */
    count--;
    txt99[count]    = (TXT99*)((unsigned)txt99[count] | 0x80000000);

    /* construct the request block for dynamic allocation */
    rb99.len        = sizeof(RB99);
    rb99.request    = S99VRBAL;
    rb99.flag1      = S99NOCNV;
    /* A caller that names a unit or a volume gets an answer, not a wait:
       without S99NOMNT a volume that is not mounted sends SVC 99 into
       allocation recovery (IEF238D), and the task stops until an operator
       replies (#181).  The same rule as __fpnew() (#172); without either,
       the request stays the one it always was.  MOUNT asks for the wait:
       a tape, or a volume the operator is to mount. */
    if ((dsalc->unit || dsalc->volser) && !dsalc->mount)
        rb99.flag1 |= S99NOMNT;
    rb99.txtptr     = txt99;

    /* SVC 99 */
    err = __svc99(&rb99);
#if 0 /* debugging */
    if (err) {
        wtof("%s: __svc99() err=%d", __func__, err);
        wtodumpf(&rb99, sizeof(RB99), "%s RB99", __func__);
    }
#endif
    if (err) goto quit;

    if (ddname) {
        /* return DDNAME */
        memcpy(ddname, txt99[0]->text, 8);
        ddname[8] = 0;
    }

quit:
    if (temp)   free(temp);
    
    /* restore the strtok() "old" pointer */
    crt->crtstrtk = crtstrtk;

    if (txt99) FreeTXT99Array(&txt99);

	// wtof("%s: exit err=%d", __func__, err);
    return err;
}
