/* @@FPNEW.C */
#include <stdio.h>
#include "src/internal/tok.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stddef.h>
#include <mvs/dynalloc.h>
#include "mvs/dynalloc.h"
#include "ext/array.h"

int
__fpnew(FILE *fp)
{
    int         err     = 1;
    unsigned    count   = 0;
    char        *recfm  = getenv("DATASET_RECFM");
    char        *lrecl  = getenv("DATASET_LRECL");
    char		*blksize= getenv("DATASET_BLKSIZE");
    char        *space  = getenv("DATASET_SPACE");
    char        *unit   = getenv("DATASET_UNIT");
    char        *volser = getenv("DATASET_VOLSER");
    int         mount   = 0;
    char        *p      = 0;
    char 		*t;
    TXT99       **txt99 = NULL;
    RB99        rb99    = {0};
    char        temp[sizeof(fp->mode)+1];
    char        *tok_save = NULL;

	// wtof("%s: enter", __func__);
	
	memset(temp, 0, sizeof(temp));
	strcpy(temp, fp->mode);
	
	/* we want to convert our mode string "w,record,name=(val1,val2),..."
	 * to something like  "w;record;name=val1,val2;..." 
	 */
	// wtof("%s: fp->mode=\"%s\"", __func__, fp->mode);
	for(p=temp; *p; p++) {
		if (*p=='(') {
			if (*(p-1) != '=') {
				/* convert "TRK(n,n)" to "TRK=n,n" 
				 * or "nnnn(n,n)" to "nnnn=n,n" */
				*p = '=';
			}
			else {
				/* convert "name=(val1,val2)..." to "name=val1,val2..." */
				strcpy(p, p+1);
				*p = toupper(*p);
			}
			/* the list keeps its commas, and is folded to upper case like
			   the rest: "volser=(pub001,pub002)" has to reach SVC 99 as
			   PUB001,PUB002 (#172) */
			for(p++;*p; p++) {
				if (*p==')') {
					strcpy(p,p+1);
					break;
				}
				*p = toupper(*p);
			}
		}
		
		if (*p==',') *p = ';';
		else *p = toupper(*p);
	}
	// wtof("%s:     mode=\"%s\"", __func__, temp);

	for(p=__tok(temp, ";", &tok_save); p; p=__tok(NULL, ";", &tok_save)) {
		if (strstr(p, "RECFM=")) recfm = p+6;
		else if (strstr(p, "LRECL=")) lrecl = p+6;
		else if (strstr(p, "BLKSIZE=")) blksize = p+8;
		else if (strstr(p, "SPACE=")) space = p+6;
		else if (strstr(p, "UNIT=")) unit = p+5;
		else if (strstr(p, "VOLSER=")) volser = p+7;
		else if (strcmp(p, "MOUNT")==0) mount = 1;
	}

#if 0 /* debugging */
	wtof("%s: recfm=\"%s\"", __func__, recfm);
	wtof("%s: lrecl=\"%s\"", __func__, lrecl);
	wtof("%s: blksize=\"%s\"", __func__, blksize);
	wtof("%s: space=\"%s\"", __func__, space);
#endif

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
    if (!recfm) recfm = "V";
    err = __txrecf(&txt99, recfm);
    if (err) goto quit;

    /* get logical record length */
    if (!lrecl) lrecl = "255";
    err = __txlrec(&txt99, lrecl);
    // wtof("%s: __txlrec(&txt99, \"%s\") err=%d", __func__, lrecl, err);
    if (err) goto quit;

    if (strchr(recfm, 'B')) {
        /* get optional block size */
        if (blksize) {
            err = __txbksz(&txt99, blksize);
            // wtof("%s: __txbksz(&txt99, \"%s\") err=%d", __func__, blksize, err);
            if (err) goto quit;
        }
    }

    /* get optional space "[CYL|TRK],pri[,sec]," or "blksize,pri[,sec]," */
    if (space) {
		if (memcmp(space, "CYL=", 4)==0) {
			/* CYLINDERS */
			err = __txcyl(&txt99, NULL);
			// wtof("%s: __txcyl(&txt99, NULL) err=%d", __func__, err);
			space += 4;
		}
		else if (memcmp(space, "TRK=", 4)==0) {
			/* TRACKS */
			err = __txtrk(&txt99, NULL);
			// wtof("%s: __txtrk(&txt99, NULL) err=%d", __func__, err);
			space += 4;
		}
		else {
			/* BLOCKS */
			char *x = strchr(space, '=');
			if (x) *x = 0;
			err = __txblk(&txt99, space);
			// wtof("%s: __txblk(&txt99, \"%s\") err=%d", __func__, space, err);
			if (x) space = x+1;
        }

        while(*space && !isdigit(*space)) space++;
        err = __txspac(&txt99, space);  /* "pri[,sec]" */
		// wtof("%s: __txspac(&txt99, \"%s\") err=%d", __func__, space, err);
        if (err) goto quit;
    }

    /* SPACE=(,,RLSE), on request (#167) - see the note in @@fpold.c for why
       this has to sit on the DD that fopen() allocates.  Skipped for a PDS
       member: partial release on a PO data set takes the space the next
       member needs. */
    if ((fp->flags & _FILE_FLAG_RLSE) && !fp->member[0]) {
        err = __txrlse(&txt99, NULL);
        if (err) goto quit;
    }

    /* UNIT= and VOL=SER=, on request (#172).  Only when set and not empty:
       __txunit() and __txvols() fail on an empty value, and without either
       the request has to stay what it always was. */
    if (unit && *unit) {
        err = __txunit(&txt99, unit);
        if (err) goto quit;
    }

    if (volser && *volser) {
        err = __txvols(&txt99, volser);   /* "vol1[,vol2...]" */
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
       replies (#172).  Only then - without either, the request stays the
       one it always was.  "mount" asks for the wait: a tape, or a volume
       the operator is to mount (#181). */
    if (((unit && *unit) || (volser && *volser)) && !mount)
        rb99.flag1 |= S99NOMNT;
    rb99.txtptr     = txt99;

    /* SVC 99 */
    err = __svc99(&rb99);
    if (err) goto quit;

    /* return DDNAME */
    memcpy(fp->ddname, txt99[0]->text, 8);
    fp->flags |= _FILE_FLAG_DYNAMIC;

quit:
    if (txt99) FreeTXT99Array(&txt99);

	// wtof("%s: exit err=%d", __func__, err);
    return err;
}
