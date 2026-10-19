/* JESOPEN.C - Open JES spool datasets */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ibm/jes2/hct.h"    /* JES Checkpoint Control Table, record 3 in HASPCKPT */
#include "ibm/jes2/jct.h"    /* JES Job Control Table                            */
#include "ibm/jes2/pddb.h"   /* JES PDDB Print Datasets                          */
#include "ibm/jes2/iot.h"    /* JES IOT                                          */
#include "mvs/jes2.h"   /* JES prototypes */
#include "mvs/dd.h"     /* get_dsab()                                       */
#include "mvs/dynalloc.h"   /* __dsalcf()                                   */
#include "mvs/recovery.h"   /* ESTAE functions */
#include "ext/array.h"    /* dynamic array */
#include "mvs/wto.h"    /* wtof                                             */
#include "src/mvs/jes2/jesvol.h"  /* __jesvol()                             */

/* Where the data sets come from (#142).  A DD in the step's JCL wins, as it
 * always did: HASPCKPT for the checkpoint, HASPACE1 for the spool.  Without
 * it, the data set is allocated by name.  JES2 builds both names from one
 * prefix, its DSNPRFX parameter: PREFIX.HASPCKPT and PREFIX.HASPACE, SYS1
 * unless the site set another.  The checkpoint is looked up in the catalog
 * and, when it is not cataloged, found by OBTAIN on the online volumes; the
 * spool needs no search, the checkpoint names its volume (_SPOOL).
 *
 * The DDs this file allocates are unallocated by __cpclos()/__jsclos(), on
 * jesclose() and on their own open failures.                              */
#define CKPTDSN     "SYS1.HASPCKPT"     /* JES2's default checkpoint        */
#define CKPTDD      "HASPCKPT"          /* the JCL override, checkpoint     */
#define SPOOLDD     "HASPACE1"          /* the JCL override, spool          */
#define SPOOLQ      "HASPACE"           /* last qualifier of the spool      */

typedef struct jesopn {
    const char  *ckptdsn;               /* checkpoint, NULL for the default */
    JES         *jes;                   /* the handle, or NULL              */
} JESOPN;

static void try_jesopen(JESOPN *opn);

JES *jesopen(void)
{
    return jesopen_ex(NULL);
}

JES *jesopen_ex(const char *ckptdsn)
{
    JESOPN      opn;
    char        name[45];
    size_t      len;
    size_t      i;

    opn.ckptdsn = NULL;
    opn.jes     = NULL;

    if (ckptdsn) {
        len = strlen(ckptdsn);
        if (len == 0 || len >= sizeof(name)) {
            wtof("Invalid checkpoint dataset name for jesopen_ex()");
            goto quit;
        }
        /* OBTAIN and the catalog know upper case names only */
        for (i = 0; i <= len; i++) name[i] = (char)toupper(ckptdsn[i]);
        opn.ckptdsn = name;
    }

    try(try_jesopen, &opn);

quit:
    return opn.jes;
}

/* the spool volume the checkpoint names, as a string ("" if none) */
static void spool_volser(const HASPCP *cp, char vol[7])
{
    int     i;

    memcpy(vol, cp->hct._SPOOL, 6);
    vol[6] = 0;
    for (i = 5; i >= 0 && (vol[i] == ' ' || vol[i] == 0); i--) vol[i] = 0;
}

/* PREFIX.HASPCKPT -> PREFIX.HASPACE: the last qualifier is replaced */
static int spool_dsn(const char *ckptdsn, char dsn[45])
{
    const char  *dot    = strrchr(ckptdsn, '.');
    size_t      len;

    if (!dot) return -1;
    len = (size_t)(dot - ckptdsn) + 1;
    if (len + sizeof(SPOOLQ) > 45) return -1;
    memcpy(dsn, ckptdsn, len);
    memcpy(dsn + len, SPOOLQ, sizeof(SPOOLQ));
    return 0;
}

/* Allocate dsn with DISP=SHR and return "DD:ddname" in ddarg.  On volser
 * when one is given; else through the catalog and, when that fails, on the
 * first online volume that has the data set.
 * 0 allocated, 1 found nowhere, -1 the allocation failed.                 */
static int alloc_dd(const char *dsn, const char *volser, char ddarg[12])
{
    char    dd[9]   = "";
    char    vol[7];
    int     rc;

    if (volser && volser[0]) {
        rc = __dsalcf(dd, "dsname=%s,disp=shr,volser=%s", dsn, volser);
    }
    else if (__dsalcf(dd, "dsname=%s,disp=shr", dsn) == 0) {
        rc = 0;
    }
    else if (__jesvol(dsn, vol, NULL) == 0) {
        rc = __dsalcf(dd, "dsname=%s,disp=shr,volser=%s", dsn, vol);
    }
    else {
        wtof("%s is not cataloged and is on no online volume", dsn);
        return 1;
    }

    if (rc) {
        wtof("Unable to allocate dataset %s, rc=%d", dsn, rc);
        return -1;
    }
    snprintf(ddarg, 12, "DD:%s", dd);
    return 0;
}

static HASPCP *open_checkpoint(const char *ckptdsn)
{
    HASPCP  *cp;
    char    ddarg[12];
    int     rc;

    if (!ckptdsn && get_dsab(NULL, CKPTDD)) {
        cp = checkpoint_open("DD:" CKPTDD);
        if (!cp) wtof("Unable to open checkpoint dataset DD:" CKPTDD);
        return cp;
    }

    rc = alloc_dd(ckptdsn ? ckptdsn : CKPTDSN, NULL, ddarg);
    if (rc > 0 && !ckptdsn) {
        /* both causes: the next reader must not chase only one */
        wtof("JES2 uses another dataset name prefix than SYS1, or "
             "there is no JES2 checkpoint");
        wtof("Use a HASPCKPT DD or jesopen_ex() with the checkpoint name");
    }
    if (rc) return NULL;

    /* on an open failure __cpopen() unallocates the DD itself.  It does
       not when it cannot get storage for its handle; the DD then stays
       until the step ends, which is better than freeing a name another
       task of this step may have been given in the meantime */
    cp = checkpoint_open(ddarg);
    if (!cp) wtof("Unable to open checkpoint dataset %s",
                  ckptdsn ? ckptdsn : CKPTDSN);
    return cp;
}

static HASPJS *open_spool(const char *ckptdsn, const HASPCP *cp)
{
    HASPJS  *js;
    char    dsn[45];
    char    vol[7];
    char    ddarg[12];

    if (!ckptdsn && get_dsab(NULL, SPOOLDD)) {
        js = spool_open("DD:" SPOOLDD);
        if (!js) wtof("Unable to open spool dataset DD:" SPOOLDD);
        return js;
    }

    if (spool_dsn(ckptdsn ? ckptdsn : CKPTDSN, dsn)) {
        wtof("No spool dataset name for checkpoint %s", ckptdsn);
        return NULL;
    }
    spool_volser(cp, vol);
    if (alloc_dd(dsn, vol, ddarg)) return NULL;

    js = spool_open(ddarg);
    if (!js) wtof("Unable to open spool dataset %s on %s", dsn, vol);
    return js;
}

__asm__("\n&FUNC    SETC 'try_jesopen'");
static void try_jesopen(JESOPN *opn)
{
    JES         *jes    = NULL;
    HASPJS      *js     = NULL;

    jes = calloc(1, sizeof(JES));
    if (!jes) {
        wtof("Unable to allocate storage for JES handle");
        goto quit;
    }

    jes->cp = open_checkpoint(opn->ckptdsn);
    if (!jes->cp) {
        jesclose(&jes);
        goto quit;
    }

    js = open_spool(opn->ckptdsn, jes->cp);
    if (!js) {
        jesclose(&jes);
        goto quit;
    }

    /* Add spool handle to array of spool handle in our JES handle.
     *
     * Unchecked, this was the one allocation on the open path that could
     * fail and still hand back a handle (#108): jes->js stays NULL while
     * the eye catcher and jes->cp say the handle is good.  jesjob() and
     * jesprint() then evaluate jes->js[0], and on MVS that load SUCCEEDS -
     * low-address protection stops stores into page zero, not fetches - so
     * they get a non-NULL value out of the PSA, walk past __jsrd4()'s own
     * NULL test and store through it.  The S0C4 the caller sees is that
     * store, not the shortage that caused it.
     *
     * jesclose() cannot release the spool handle here because it reaches it
     * through the array that does not exist, so close it directly first.
     * That is not a double close: if arrayadd() failed the array holds no
     * elements, and jesclose() only walks the ones it counts.             */
    if (arrayadd(&jes->js, js)) {
        wtof("Unable to add spool handle to JES handle");
        spool_close(js);
        jesclose(&jes);
        goto quit;
    }

quit:
    opn->jes = jes;
    return;
}
