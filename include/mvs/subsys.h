#ifndef MVS_SUBSYS_H
#define MVS_SUBSYS_H
#include <sys/_cc370.h>
/* mvs/subsys.h - subsystems: SSVT, SSCT, SSIB.
**
** libc370 2.0 merges clibssvt.h, clibssct.h and clibssib.h into this header (#256).
** Their declarations are unchanged and appear in dependency order.
*/

/* ---- 1.x clibssvt.h -------------------------------------------------- */
typedef struct ssvt     SSVT;       /* subsystem vector table               */

struct ssvt {
    short       ssvtrsv1;           /* 00 RESERVED                          */
    short       ssvtfnum;           /* 02 NUMBER OF FUNCTIONS SUPPORTED BY
                                          THIS SUBSYSTEM                    */
/*
**  256 BYTE FUNCTION MATRIX -
**
**    THE SSOB FUNCTION ID MINUS ONE IS USED AS AN OFFSET INTO
**    THIS MATRIX.
**
**        MATRIX FUNCTION BYTE  =0 : THE FUNCTION SPECIFIED IN THE
**                                   SSOB IS NOT SUPPORTED BY THIS
**                                   SUBSYSTEM.
**        MATRIX FUNCTION BYTE ¬=0 : THE VALUE (FUNCTION BYTE-1)*4
**                                   IS ADDED TO THE ADDRESS OF
**                                   SSVTFRTN TO OBTAIN THE
**                                   ADDRESS OF THE WORD CONTAINING
**                                   THE FUNCTION ROUTINE POINTER FOR
**                                   THIS REQUEST.
*/
    char        ssvtfcod[256];      /* 04 FUNCTION MATRIX                   */
    /* note: funcnum is used with ssvtfcod[] */

    void        *ssvtfrtn[0];       /* 104 SSVTFRTN IS THE FIRST WORD OF A
                                           VARIABLE LENGTH MATRIX CONTAINING
                                           FUNCTION ROUTINE POINTERS FOR
                                           FUNCTIONS SUPPORTED BY THIS
                                           SUBSYSTEM.  THE MATRIX CAN BE A
                                           MAXIMUM OF 256 WORDS LONG.       */
    /* note: index is used with ssvtfrtn[] */
};

/* ssvt_new() allocate a SSVT area with space for funcmax functions, requires supervisor state */
SSVT * ssvt_new(unsigned funcmax)                               asm("@@SVNEW");

/* ssvt_free() deallocates a SSVT area, requires supervisor state */
void ssvt_free(SSVT *ssvt)                                      asm("@@SVFREE");

/* ssvt_set() places func into SSVT area, requires supervisor state */
/* note: index range is 1 - funcmax inclusive */
int ssvt_set(SSVT *ssvt, unsigned index, void *func)            asm("@@SVSET");

/* ssvt_reset() removes func from SSVT area, requires supervisor state */
static __inline int ssvt_reset(SSVT *ssvt, unsigned index)
{
    return ssvt_set(ssvt, index, (void*)0);
}

/* ssvt_funcmap() maps function index to subsystem function number */
/* note: it's possible to have a function (index) that serves multiple function numbers */
/* note: index range is 0 - ssvt->ssvtfnum inclusive (0 disables the funcnum function) */
/* note: funcnum range is 1 - 256 inclusive */
int ssvt_funcmap(SSVT *ssvt, unsigned index, unsigned funcnum)  asm("@@SVFMAP");

/* ---- 1.x clibssct.h -------------------------------------------------- */
typedef struct ssct     SSCT;   /* subsystem CVT, SP=241 KEY=0              */
/* Note: The address of the first SSCT is in CVTJESCT at offset 0x18        */
/*       ssct = cvt->cvtjesct->jesssct                                      */

struct ssct {
    char        ssctid[4];      /* 00 CL4'SSCT' CONTROL BLOCK IDENTIFIER    */
#define SSCT_EYE    "SSCT"      /* ...                                      */
    SSCT        *ssctscta;      /* 04 PTR TO NEXT SSCVT OR ZERO             */
    char        ssctsnam[4];    /* 08 SUBSYSTEM NAME                        */
    char        ssctflg1;       /* 0C FLAGS                                 */
#define SSCTSFOR    0x80        /* ... SERIAL FIB OPERATIONS REQUIRED       */
#define SSCTUPSS    0x40        /* ... USE PRIMARY SUBSYSTEM'S
                                       SERVICES FOR THIS SUBSYSTEM          */
    char        ssctssid;       /* 0D SUBSYSTEM IDENTIFIER. SET BY
                                      SUBSYSTEM FIRST TIME IT IS
                                      INVOKED AFTER IPL                     */
#define SSCTUNKN    0x00        /* ... UNKNOWN SUBSYSTEM ID                 */
#define SSCTJES2    0x02        /* ... JES2 SUBSYSTEM ID                    */
#define SSCTJES3    0x03        /* ... JES3 SUBSYSTEM ID                    */
    char        ssctrsv1[2];    /* 0E RESERVED                              */
    SSVT        *ssctssvt;      /* 10 SUBSYSTEM VECTOR TABLE POINTER        */
    void        *ssctsuse;      /* 14 RESERVED FOR SUBSYSTEM USAGE          */
};

/* ssct_new() allocate SSCT storage, requires supervisor state */
SSCT *ssct_new(const char *name, SSVT *ssvt, void *suse)            asm("@@SSNEW");

/* ssct_free() deallocate SSCT storage, requires supervisor state */
void ssct_free(SSCT *ssct)                                          asm("@@SSFREE");

/* ssct_find() find subsystem storage by name */
SSCT *ssct_find(const char *name)                                   asm("@@SSFIND");

/* ssct_install() install SSCT as subsystem after after_name subsystem, requires supervisor state */
/* note: when after_name is NULL ssct is placed after the first subsystem, usually "JES2" or "MSTR" */
/* note: when after_name is not found, ssct is placed after the last subsystem */
int ssct_install(SSCT *ssct, const char *after_name)                asm("@@SSINST");

/* ssct_remove() remove SSCT as subsystem, requires supervisor state and key 0 */
int ssct_remove(SSCT *ssct)                                         asm("@@SSREM");

/* ssct_remove_by_name() remove SSCT as subsystem, requires supervisor state and key 0 */
int ssct_remove_by_name(const char *name)                           asm("@@SSREMN");

/* ---- 1.x clibssib.h -------------------------------------------------- */
#include <ibm/mvs/iefjssib.h>		/* SSIB struct */

/* __ssib() - returns pointer to job SSID */
SSIB * __ssib(void)										asm("@@SSIB");

/* __jobid() - returns pointer to 8 character SSIBJBID in job SSIB */
const char * __jobid(void)								asm("@@JOBID");

/* ---- the subsystem request, from 1.x iefssobh.h (#278) ----------------- */
#include <ibm/mvs/iefssobh.h>		/* SSOB struct */

/* iefssreq() - pass an SSOB to the subsystem interface (IEFSSREQ) */
extern int iefssreq(SSOB *ssob);

#endif /* MVS_SUBSYS_H */
