/*
 * tstjesda.c - libc370 #142: where do the JES2 checkpoint and spool come
 * from, if not from a DD in the caller's JCL?
 *
 * ISSUE #142: jesopen() (src/jes/jesopen.c:37,46) reaches both data sets by
 * ddname - checkpoint_open("DD:HASPCKPT"), spool_open("DD:HASPACE1") - so
 * every caller's JCL has to carry the site's UNIT and VOL=SER.  That is how
 * mvslovers/httpd#256 happened.  What the issue does NOT say is that
 * __cpopen()/__jsopen() already branch on the "DD:" prefix and dynalloc the
 * data set themselves when the argument is a plain name.  The code is
 * written; jesopen() is the only reason it never runs.
 *
 * MEASURED BEFORE THIS PROBE, so it is not re-measured here:
 *   MVSCE (mvsdev), JOB02606: SYS1.HASPCKPT and SYS1.HASPACE are both
 *     cataloged (MVS000 / SPOOL1) and ALLOC DA(...) SHR with no VOL works.
 *   TK5 (drnmig3a), JOB00029: neither is cataloged - IKJ56228I on the ALLOC,
 *     IDC3012I ENTRY NOT FOUND on the LISTCAT.
 * So the catalog route is real but not sufficient: it carries MVSCE and
 * misses TK5.  This probe measures what else is available, on the stand it
 * runs on, and prototypes the one route that could replace the DDs.
 *
 * ====================================================================
 * WHAT EACH CASE DECIDES
 * --------------------------------------------------------------------
 * (1) The SSVT route.  ssct_find("JES2") reaches the SSCT in CSA and
 *     HASPSVT.svhct (+0x17C) is documented as "ADDRESS OF HASP HCT".  If
 *     that HCT were readable from an ordinary address space, every other
 *     route here is unnecessary: _CHKPT, _SPOOL and _NUMDA would come
 *     live from JES2 on any stand, cataloged or not.  The expectation is
 *     that it is NOT - the HCT lives in the JES2 private area and 3.8j has
 *     no cross-memory - in which case the fetch SUCCEEDS and returns this
 *     address space's own storage.  That is the trap: it does not abend,
 *     it lies.  Case (6) is what settles it.
 * (2) __locate() - the catalog, programmatically.  Expect rc=0 + volser on
 *     MVSCE, nonzero on TK5.
 * (3) checkpoint_open("SYS1.HASPCKPT") - the dead code path in @@cpopen.c,
 *     called directly for the first time.  This is #142's minimal form.
 * (4) __listvl() + __dscbdv() - OBTAIN SEARCH by DSN over the online
 *     volumes.  Finds an uncataloged data set, so this is the candidate
 *     route for TK5.  The cost (volumes scanned) is printed, because a
 *     per-open VTOC sweep is only acceptable if it is small.
 * (5) Allocate by DSN+VOL from (4)'s answer and open the checkpoint through
 *     the returned ddname - the full bootstrap, with no JCL DD anywhere.
 *     Prints _CHKPT/_CHKPT2/_SPOOL/_NUMDA from the on-disk HCT.  _SPOOL is
 *     the point: once the checkpoint is open the spool needs no discovery
 *     of its own.
 * (6) VERDICT: byte-compare the 204 bytes at svhct against the on-disk HCT
 *     from (5).  Equal = the SSVT route is real.  Different = it is reading
 *     this address space, and must not enter the design.
 * ====================================================================
 *
 * SETUP: none.  Everything is read-only: OBTAIN, LOCATE, DISP=SHR
 * allocations that are freed again, and reads of the checkpoint.  No DD is
 * required - that is the whole point - but if HASPCKPT is allocated anyway
 * the probe ignores it.
 *
 * BUILD (host):
 *     cc370 -O1 -Iinclude test/mvs/tstjesda.c -o TSTJESDA \
 *           -flinker-output=xmit
 * Install: RECEIVE the XMIT into the STEPLIB of jcl/tstjesda.jcl.
 *
 * RC: 0 = the probe ran and printed its measurements.  This is a probe, not
 * a regression test: the interesting output is the case text, not the COND
 * CODE.  8 means a case could not run at all (no JES2 SSCT, no storage).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "clibssct.h"   /* ssct_find(), SSCT                                */
#include "haspsvt.h"    /* HASPSVT, svhct                                   */
#include "hasphct.h"    /* __HCT - the checkpoint master record             */
#include "clibcp.h"     /* checkpoint_open(), HASPCP                        */
#include "cliblist.h"   /* __listvl(), VOLLIST                              */
#include "clibdscb.h"   /* __locate(), __dscbdv(), LOCWORK, DSCB            */
#include "clibary.h"    /* arraycount()                                     */
#include "clibio.h"     /* __dsalcf(), __dsfree()                           */
#include "clibtry.h"    /* try(), tryrc()                                   */
#include "clibwto.h"    /* wtof()                                           */

#define CKPTDSN     "SYS1.HASPCKPT"
#define SPOOLDSN    "SYS1.HASPACE"

static int  bad = 0;

/* ------------------------------------------------------------------ */

static void hexdump(const char *what, const void *p, unsigned len)
{
    const unsigned char *b = (const unsigned char *)p;
    unsigned             i, j;

    printf("    %s:\n", what);
    for (i = 0; i < len; i += 16) {
        printf("      %04X  ", i);
        for (j = 0; j < 16; j++) {
            if (i + j < len) printf("%02X", b[i + j]);
            else             printf("  ");
            if ((j & 3) == 3) printf(" ");
        }
        printf(" *");
        for (j = 0; j < 16 && i + j < len; j++) {
            unsigned char c = b[i + j];
            /* EBCDIC: let the compiler encode the guards, never a code */
            printf("%c", (c >= ' ' && c != 0xFF) ? c : '.');
        }
        printf("*\n");
    }
}

/* print a volser that is NOT NUL terminated */
static void putvol(const char *what, const char *v, unsigned n)
{
    unsigned i;
    printf("      %-24s '", what);
    for (i = 0; i < n; i++) printf("%c", (v[i] >= ' ') ? v[i] : '.');
    printf("'\n");
}

/* ------------------------------------------------------------------ */
/* a fetch that may be reading nothing at all - do it under try()      */

typedef struct xcopy XCOPY;
struct xcopy {
    const void  *from;
    void        *to;
    unsigned     len;
    int          done;
};

static int docopy(void *p)
{
    XCOPY *x = (XCOPY *)p;

    memcpy(x->to, x->from, x->len);
    x->done = 1;
    return 0;
}

/* ------------------------------------------------------------------ */

int main(void)
{
    SSCT        *ssct       = NULL;
    HASPSVT     *svt        = NULL;
    void        *hctp       = NULL;
    HASPCP      *cp         = NULL;
    VOLLIST     **vl        = NULL;
    LOCWORK      locw;
    DSCB         dscb;
    XCOPY        x;
    __HCT        live;                  /* HCT as read through svhct    */
    __HCT        disk;                  /* HCT as read from the ckpt DS */
    char         foundvol[7]   = {0};
    char         ddname[9]     = {0};
    unsigned     nvol          = 0;
    unsigned     nobtain       = 0;
    int          havelive      = 0;
    int          havedisk      = 0;
    int          rc;
    unsigned     i;

    printf("TSTJESDA - libc370 #142: finding the JES2 checkpoint "
           "without a DD\n\n");
    wtof("TSTJESDA start");

    memset(&live, 0, sizeof(live));
    memset(&disk, 0, sizeof(disk));

    /* ---------------------------------------------------------- (1) */
    printf("(1) SSVT route: ssct_find(\"JES2\") -> svhct\n");
    ssct = ssct_find("JES2");
    if (!ssct) {
        printf("    no JES2 SSCT - nothing to measure here\n");
        bad = 1;
    }
    else {
        printf("    SSCT           = %08X\n", (unsigned)ssct);
        printf("    ssctssvt       = %08X\n", (unsigned)ssct->ssctssvt);
        printf("    ssctssid       = %02X\n", (unsigned char)ssct->ssctssid);
        svt = (HASPSVT *)ssct->ssctssvt;
        if (!svt) {
            printf("    SSVT is NULL - JES2 not initialised?\n");
        }
        else {
            /* the header documents svhct at +0x17C; say what we compiled */
            printf("    offsetof(svhct) = %03X (header says 17C)\n",
                   (unsigned)((char *)&svt->svhct - (char *)svt));
            hctp = svt->svhct;
            printf("    svhct          = %08X\n", (unsigned)hctp);

            if (!hctp) {
                printf("    svhct is NULL - route is dead\n");
            }
            else {
                x.from = hctp;
                x.to   = &live;
                x.len  = sizeof(live);
                x.done = 0;
                rc = try(docopy, &x);
                if (x.done) {
                    havelive = 1;
                    printf("    fetch of %u bytes SUCCEEDED "
                           "(this proves nothing yet - see (6))\n",
                           (unsigned)sizeof(live));
                    hexdump("first 64 bytes at svhct", &live, 64);
                }
                else {
                    printf("    fetch ABENDED, try rc=%d code=%08X\n",
                           rc, tryrc());
                    printf("    -> route is not usable, and it says so "
                           "loudly.  That is the good outcome.\n");
                }
            }
        }
    }
    printf("\n");

    /* ---------------------------------------------------------- (2) */
    printf("(2) catalog route: __locate(\"%s\")\n", CKPTDSN);
    memset(&locw, 0, sizeof(locw));
    rc = __locate(CKPTDSN, &locw);
    printf("    rc = %d\n", rc);
    if (rc == 0) putvol("volser", locw.volser, 6);
    else         printf("      not in the catalog on this stand\n");
    printf("\n");

    /* ---------------------------------------------------------- (3) */
    printf("(3) the dead path in @@cpopen.c: checkpoint_open(\"%s\")\n",
           CKPTDSN);
    cp = checkpoint_open(CKPTDSN);
    if (cp) {
        printf("    OPENED - #142's minimal form works on this stand\n");
        putvol("hct._SPOOL", (char *)cp->hct._SPOOL, 6);
        checkpoint_close(cp);
        cp = NULL;
    }
    else {
        printf("    FAILED - expected where the data set is uncataloged\n");
    }
    printf("\n");

    /* ---------------------------------------------------------- (4) */
    printf("(4) volume scan: __listvl() + __dscbdv(\"%s\", vol)\n", CKPTDSN);
    vl = __listvl(NULL, 0, NULL);
    if (!vl) {
        printf("    __listvl() returned NULL\n");
        bad = 1;
    }
    else {
        nvol = arraycount(&vl);
        printf("    %u volumes online\n", nvol);
        for (i = 0; i < nvol; i++) {
            memset(&dscb, 0, sizeof(dscb));
            nobtain++;
            rc = __dscbdv(CKPTDSN, vl[i]->volser, &dscb);
            if (rc == 0) {
                memcpy(foundvol, vl[i]->volser, 6);
                foundvol[6] = 0;
                printf("    HIT  %-6s  (OBTAIN rc=0) after %u OBTAINs\n",
                       foundvol, nobtain);
                break;
            }
        }
        if (!foundvol[0])
            printf("    not found on any of the %u volumes\n", nvol);

        /* the spool, for the record - it is NOT how we would find it */
        for (i = 0; i < nvol; i++) {
            memset(&dscb, 0, sizeof(dscb));
            if (__dscbdv(SPOOLDSN, vl[i]->volser, &dscb) == 0) {
                putvol("SYS1.HASPACE lives on", vl[i]->volser, 6);
                break;
            }
        }
        printf("    OBTAINs issued for the checkpoint: %u of %u volumes\n",
               nobtain, nvol);
        __freevl(&vl);
        vl = NULL;
    }
    printf("\n");

    /* ---------------------------------------------------------- (5) */
    printf("(5) full bootstrap: allocate %s by DSN+VOL, then open it\n",
           CKPTDSN);
    if (!foundvol[0]) {
        printf("    skipped - (4) found no volume\n");
    }
    else {
        rc = __dsalcf(ddname, "dsname=%s,disp=shr,volser=%s",
                      CKPTDSN, foundvol);
        printf("    __dsalcf(volser=%s) rc=%d ddname=\"%s\"\n",
               foundvol, rc, ddname);
        if (rc == 0 && ddname[0]) {
            char ddarg[16];

            sprintf(ddarg, "DD:%s", ddname);
            cp = checkpoint_open(ddarg);
            if (!cp) {
                printf("    checkpoint_open(\"%s\") FAILED\n", ddarg);
            }
            else {
                printf("    OPENED through %s - no JCL DD involved\n", ddarg);
                memcpy(&disk, &cp->hct, sizeof(disk));
                havedisk = 1;

                putvol("_CHKPT  (ckpt vol)", (char *)disk._CHKPT, 6);
                putvol("_CHKPT2 (2nd ckpt)", (char *)disk._CHKPT2, 6);
                putvol("_SPOOL  (spool vol)", (char *)disk._SPOOL, 6);
                printf("      %-24s %u\n", "_NUMDA  (spool vols)",
                       (unsigned)disk._NUMDA);
                printf("      %-24s %u\n", "_NUMTGV (trkgrp/vol)",
                       (unsigned)disk._NUMTGV);
                hexdump("first 64 bytes of the on-disk HCT", &disk, 64);

                checkpoint_close(cp);
                cp = NULL;
            }
            __dsfree(ddname);
        }
    }
    printf("\n");

    /* ---------------------------------------------------------- (6) */
    printf("(6) VERDICT: is svhct the real HCT?\n");
    if (!havelive) {
        printf("    no storage was fetched at svhct - nothing to compare.\n");
        printf("    -> the SSVT route is unusable here.\n");
    }
    else if (!havedisk) {
        printf("    the on-disk HCT was never read - cannot decide.\n");
        bad = 1;
    }
    else if (memcmp(&live, &disk, sizeof(live)) == 0) {
        printf("    IDENTICAL over %u bytes.\n", (unsigned)sizeof(live));
        printf("    -> svhct really does reach the HCT.  The checkpoint "
               "and spool volumes\n");
        printf("       can be read live from JES2, on any stand, with no "
               "catalog and no scan.\n");
    }
    else {
        unsigned diff = 0;
        const unsigned char *a = (const unsigned char *)&live;
        const unsigned char *b = (const unsigned char *)&disk;

        for (i = 0; i < sizeof(live); i++) if (a[i] != b[i]) diff++;
        printf("    DIFFERENT in %u of %u bytes.\n",
               diff, (unsigned)sizeof(live));
        printf("    -> svhct points into the JES2 private area.  The fetch "
               "succeeded because\n");
        printf("       it read THIS address space at that address.  The "
               "route must not be used.\n");
    }

    printf("\nTSTJESDA done, rc=%d\n", bad ? 8 : 0);
    wtof("TSTJESDA done rc=%d", bad ? 8 : 0);
    return bad ? 8 : 0;
}
