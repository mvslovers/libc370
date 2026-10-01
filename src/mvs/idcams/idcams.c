#include <mvs/idcams.h>
#include <mvs/link.h>
#include <mvs/wto.h>
#include <s370/savearea.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static int link_idcams(const char *cmds, IDCAMS_SYSPRINT fn, void *arg);

__asm__("\n&FUNC    SETC 'idcams'");
int idcams(const char *fmt, ...)
{
    va_list     args;
    char        cmds[256];

    va_start(args, fmt);
    vsnprintf(cmds, sizeof(cmds), fmt, args);
    va_end(args);
    cmds[sizeof(cmds)-1] = 0;

    return link_idcams(cmds, 0, 0);
}

/* idcams_sysprint() - idcams(), with every SYSPRINT line handed to fn (#71) */
__asm__("\n&FUNC    SETC 'idcams_sysprint'");
int idcams_sysprint(IDCAMS_SYSPRINT fn, void *arg, const char *fmt, ...)
{
    va_list     args;
    char        cmds[256];

    va_start(args, fmt);
    vsnprintf(cmds, sizeof(cmds), fmt, args);
    va_end(args);
    cmds[sizeof(cmds)-1] = 0;

    return link_idcams(cmds, fn, arg);
}

typedef struct {
    char        op;
#define OP_OPEN     0
#define OP_CLOSE    4
#define OP_GET      8
#define OP_PUT      12

    char        opt;
#define OPT_INPUT   0x80
#define OPT_OUTPUT  0x40
#define OPT_DDNAME  0x20
#define OPT_DSNAME  0x10

    short       msgno;
#define MSGNO_NORMAL  0 /* normal output, otherwise IDCnnnn message number */
} IOFLAGS;

typedef struct {
    char        name[44];   /* OPEN DDNAME or DSNAME, CLOSE DSNAME */
} OPEN, CLOSE;

typedef struct {
    char        *rec;       /* pointer to record */
    int         reclen;     /* length of record */
} GET, PUT;

typedef union {
    OPEN        open;
    CLOSE       close;
    GET         get;
    PUT         put;
} IOINFO;

typedef struct {
    void        *udata;
    IOFLAGS     *ioflags;
    IOINFO      *ioinfo;
} IOEXIT;

typedef struct {
    short       len;        /* length of data */
    char        data[256];
} OPTIONS;

typedef struct {
    short       len;            /* length of struct */
    char        __1[8];         /* must be binary zeros */
    char        __2[8];         /* must be binary zeros */
    char        __3[3];         /* must be binary zeros */
    char        __4[8];         /* must be binary zeros */
    char        sysin[8];       /* zeros or dd name as sysin */
    char        sysprint[8];    /* zeros or dd name as sysprint */
} DDNAMES;

typedef struct {
    void        *ddname;        /* "DD" + ddname space filled */
    void        *iofunc;        /* address of IO function */
    void        *udata;         /* user data passed to IO function */
} IODD;

typedef struct {
    int         count;          /* number of IO's that follow */
    IODD        iodd[2];        /* IODD structs */
} IOLIST;

typedef struct {
    void        *options;       /* options */
    void        *ddnames;       /* dd names */
    void        *pages;         /* pages */
    void        *iolist;        /* io list */
    void        *auxlist;       /* aux list */
} PLIST;

typedef struct {
    void            *nab;
    const char      *cmds;
    IDCAMS_SYSPRINT fn;         /* idcams_sysprint(): each SYSPRINT line */
    void            *arg;       /* ... passed to fn */
} UDATA;

static const short none = 0;

int __idcex(UDATA *udata, IOFLAGS *ioflags, IOINFO *ioinfo);
int __idcexc(UDATA *udata, IOFLAGS *ioflags, IOINFO *ioinfo);

__asm__("\n&FUNC    SETC 'link_idcams'");
static int link_idcams(const char *cmds, IDCAMS_SYSPRINT fn, void *arg)
{
    char        ddname1[10] = "DDSYSIN   ";
    char        ddname2[10] = "DDSYSPRINT";
    void        *iofunc     = __idcex;
    unsigned    stack[2000] = {0};
    UDATA       udata       = {(void*)stack, cmds, fn, arg};
    IODD        sysin       = {ddname1, iofunc, &udata};
    IODD        sysprint    = {ddname2, iofunc, &udata};
    IOLIST      iolist      = {2, sysin, sysprint};
    int         prc         = 0;
    int         rc;
    unsigned    plist[4];

#if 0
	wtof("idcams: link_idcams(\"%s\")", cmds);
	wto_traceback((void*)0);
/*
    wtodumpf((char*)cmds, strlen(cmds), "CMDS");
    wtodumpf(&iolist, sizeof(iolist), "IOLIST");
    wtodumpf(&sysin, sizeof(sysin), "SYSIN");
    wtodumpf(ddname1, sizeof(ddname1), "DDNAME1");
    wtodumpf(&sysprint, sizeof(sysprint), "SYSPRINT");
    wtodumpf(ddname2, sizeof(ddname2), "DDNAME2");
    wtodumpf(&udata, sizeof(udata), "UDATA");
    wtof("iofunc=%08X", iofunc);
*/
#endif

    plist[0]    = (unsigned)&none;      /* options list */
    plist[1]    = (unsigned)&none;      /* ddnames list */
    plist[2]    = (unsigned)&none;      /* page number list */
    plist[3]    = ((unsigned)&iolist) + 0x80000000;    /* io list, end of plist */

    rc = __linkds("IDCAMS", NULL, plist, &prc);

    /* if the link failed, return that as the return code */
    if (rc > 0) prc = rc * -1;  /* make negative */
    if (rc < 0) prc = rc;       /* already negative */

quit:
    return prc;
}

__asm("ENTRY @@IDCEX\n"
"@@IDCEX\tDS\t0H\n"
"         SAVE  (14,12),,@@IDCEX\n"
"         LA    12,0(,15)           base register\n"
"         USING @@IDCEX,12          base register\n"
"         L     14,0(,1)            A(UDATA)\n"
"         L     15,0(,14)           A(NAB in C stack frame)\n"
"         ST    13,4(,15)           backward chain (prev)\n"
"         ST    15,8(,13)           forward chain (next)\n"
"         LR    13,15               our stack frame\n"
"         LA    15,88(,15)          minimal stack frame\n"
"         ST    15,76(13)           next available byte\n"
"* Now call our idcams io exit\n"
"         L     15,=A(@@IDCEXC)     A(__idcexc)\n"
"         BALR  14,15               call our C function\n"
"         L     13,4(,13)           get prev stack frame\n"
"         RETURN (14,12),RC=(15)    restore regs and return\n"
"         LTORG ,                   our literal pool\n"
"         DROP  12                  drop base register");

/* the IDCnnnn number of a SYSPRINT record, 0 when it carries none.
**
** IDCAMS hands the exit a message number of its own (ioflags->msgno), but
** not the IDC one: measured on MVS 3.8j (#71, JOB01058), IDC3012I arrives
** as 12, IDC3203I as 203, IDC0551I as 551, and the two summaries IDC0001I
** and IDC0002I as -1 and -2 -- the leading digit is gone, so 0012 and 3012
** would look alike.  The record itself says it plainly: a carriage-control
** byte, then "IDCnnnnI".  So the number is read from there. */
static int idc_number(const char *rec, int len)
{
    int     n   = 0;
    int     i;

    if (len < 9 || rec[1] != 'I' || rec[2] != 'D' || rec[3] != 'C')
        return 0;
    for (i = 4; i < 8; i++) {
        if (rec[i] < '0' || rec[i] > '9')
            return 0;
        n = n * 10 + (rec[i] - '0');
    }
    return n;
}

__asm__("\n&FUNC    SETC '__idcexc'");
int __idcexc(UDATA *udata, IOFLAGS *ioflags, IOINFO *ioinfo)
{
    int         rc      = 0;
    const char  *cmds   = udata->cmds;

#if 0
    wtof("In %s", __func__);
    wtof("udata=%08X", udata);
    wtof("ioflags=%08X", ioflags);
    wtof("ioinfo=%08X", ioinfo);
#endif

    switch (ioflags->op) {
    case OP_OPEN:
        /* wtof("OP_OPEN %-8.8s", ioinfo->open.name); */
        /* nothing to do */
        break;
    case OP_CLOSE:
        /* wtof("OP_CLOSE %-8.8s", ioinfo->close.name); */
        /* nothing to do */
        break;
    case OP_GET:
        /* wtof("OP_GET"); */
        ioinfo->get.rec = (char*)udata->cmds;
        ioinfo->get.reclen = udata->cmds ? strlen(udata->cmds) : 0;
        if (!udata->cmds) rc = 4; /* indicate end-of-data */
        udata->cmds = 0;
        break;
    case OP_PUT:
        /* wtof("OP_PUT %*.*s", ioinfo->put.reclen, ioinfo->put.reclen, ioinfo->put.rec); */
        /* wtodumpf(ioinfo->put.rec, ioinfo->put.reclen, "IDCAMS PUT"); */
        if (udata->fn)          /* idcams_sysprint() (#71) */
            udata->fn(udata->arg, idc_number(ioinfo->put.rec, ioinfo->put.reclen),
                      ioinfo->put.rec, ioinfo->put.reclen);
        break;
    }

    /* wtof("Leaving %s, rc=%d", __func__, rc); */
    return rc;
}
