#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "mvs/recovery.h"
#include "mvs/crt.h"
#include "mvs/wto.h"
#include "src/internal/wtofmt.h"

/* The report is written without the printf family (size analysis F4):
   every threaded program links __abrpt() through cthread's cleanup, and
   one wtof() here linked all of printf and half of stdio with it.  The
   lines read as before. */

/* "<label>:XXXXXXXX:<text>" */
__asm__("\n&FUNC    SETC 'regline'");
static void
regline(const char *label, unsigned v, const char *text)
{
    char    msg[126];
    int     k = 0;

    wtofmt_str(msg, &k, sizeof(msg), label);
    wtofmt_str(msg, &k, sizeof(msg), ":");
    wtofmt_hex(msg, &k, sizeof(msg), v, 8);
    wtofmt_str(msg, &k, sizeof(msg), ":");
    wtofmt_str(msg, &k, sizeof(msg), text);
    wto(msg);
}

/* a fixed text; wto() wants a writable buffer */
__asm__("\n&FUNC    SETC 'say'");
static void
say(const char *text)
{
    char    msg[126];
    int     k = 0;

    wtofmt_str(msg, &k, sizeof(msg), text);
    wto(msg);
}

typedef struct {
    unsigned u[2];
} PARAM;

__asm__("\n&FUNC    SETC 'get_offset'");
static void
get_offset(SDWA *sdwa, char *buf)
{
    unsigned    *psw    = (unsigned*)sdwa->SDWANXT1;
    unsigned    *np     = (unsigned*)sdwa->SDWAGR13;
    unsigned    *ep;
    unsigned    len;
    unsigned    offset;
    unsigned char *cp;

    *buf = 0;
    if (np < (unsigned*)8192) goto quit;

    np = (unsigned*)np[1];  /* prev save area   */
    ep = (unsigned*)np[4];  /* R15              */
    offset = ((unsigned)psw - (unsigned)ep);

#if 0
    wtof("psw %08X, ep %08X, offset %08X", psw, ep, offset);
#endif

    if (ep > psw) goto quit;    /* can't be right */
    if ((ep[0] >> 8) == 0x0047F0F0) {
        cp  = (unsigned char*)ep;
        if (cp[4] > cp[3]) goto quit;   /* that doesn't look right */

        len = cp[4];
        if (len > 40) len = 40;         /* keep it reasonable */

        {
            int k = 0;
            unsigned n;

            wtofmt_str(buf, &k, 80, "epname ");
            for (n = 0; n < len; n++) buf[k++] = (char)cp[5 + n];
            buf[k] = 0;
            wtofmt_str(buf, &k, 80, " offset ");
            wtofmt_hex(buf, &k, 80, offset, 8);
        }
    }

quit:
    return;
}

__asm__("\n&FUNC    SETC 'get_addr'");
static void
get_addr(unsigned addr, char *buf)
{
    int         rc;
    int         i;
    int         j;
    int         pos;
    int         pos2;
    unsigned    *np;
    char        *cp;
    char        hex[12];

#if 0
    /* force a bad address to test failed() recovery */
    if (!addr) addr = 0x00654320;
#endif

    /* preload "default" values */
    strcpy(buf,
        "........ ........ ........ ........ *................*");

    /* Reject addresses outside 24-bit private storage so a stray
     * register value can't fault this storage probe and nest an abend
     * on top of the one we are reporting.  Matches the range the
     * save-area walkers use below; residual faults stay caught by the
     * try() wrappers around every get_addr() call.
     */
    if (addr < 0x00002000 || addr >= 0x00FF0000) goto quit;

    pos  = 0;
    pos2 = 37;
    np = (unsigned*)addr;   /* unsigned pointer to mem */
    cp = (char*)addr;       /* character pointer to mem */

    for(i=0; i < 4; i++, np++, pos+=9) {
        {
            int k = 0;

            wtofmt_hex(hex, &k, sizeof(hex), *np, 8);
        }
        memcpy(&buf[pos], hex, 8);

        for(j=0; j < 4; j++, cp++, pos2++) {
            if (isgraph(*cp)) {
                buf[pos2] = *cp;
            }
        }
    }

quit:
    return;
}

__asm__("\n&FUNC    SETC 'get_epname'");
static void
get_epname(unsigned addr, char *buf)
{
    unsigned    *ep = (unsigned*)addr;
    unsigned    len;

    *buf = 0;
    if ((ep[0] >> 8) == 0x0047F0F0) {
        unsigned char *cp  = (unsigned char*)ep;
        if (cp[4] > cp[3]) goto quit;   /* that doesn't look right */

        len = cp[4];
        if (len > 40) len = 40;         /* keep it reasonable */

        {
            unsigned n;

            for (n = 0; n < len; n++) buf[n] = (char)cp[5 + n];
            buf[len] = 0;
        }
    }

quit:
    return;
}

__asm__("\n&FUNC    SETC 'dump_regs'");
static int
dump_regs(SDWA *sdwa)
{
    int         i;
    unsigned    *n;
    char        buf[80];

    for(i=0, n=&sdwa->SDWAGR00; i<16; i++, n++) {
        try(get_addr, *n, buf);

        {
            char lab[4];

            lab[0] = 'R';
            lab[1] = (char)('0' + i / 10);
            lab[2] = (char)('0' + i % 10);
            lab[3] = 0;
            regline(lab, *n, buf);
        }
    }

    return 0;
}

__asm__("\n&FUNC    SETC 'get_sa_prev'");
static int
get_sa_prev(unsigned *n, unsigned **prev)
{
    /* wtof("%s stack=%08X", __func__, n); */
    if (n[1] >= 0x00002000 && n[1] < 0x00FF0000) {
        *prev = (unsigned*)n[1];
    }
    /* wtof("%s prev=%08X", __func__, *prev); */
    return 0;
}

__asm__("\n&FUNC    SETC 'get_sa_next'");
static int
get_sa_next(unsigned *n, unsigned **next)
{
    /* wtof("%s stack=%08X", __func__, n); */
    if (n[2] >= 0x00002000 && n[2] < 0x00FF0000) {
        *next = (unsigned*)n[2];
    }
    /* wtof("%s next=%08X", __func__, *next); */
    return 0;
}

__asm__("\n&FUNC    SETC 'dump_sa'");
static int
dump_sa(SDWA *sdwa)
{
    unsigned    *psa    = 0;                            /* low core == PSA      */
    unsigned    *tcb    = (unsigned*)psa[0x21c/4];      /* TCB      == PSATOLD  */
    unsigned    *fsa    = (unsigned*)(tcb[0x70/4] & 0x00FFFFFF);    /* first save area for TCB */
    unsigned    *ppa    = (unsigned*)(fsa[2]);          /* A(@PPA) */
    int         i;
    unsigned    *n;
    unsigned    *prev;
    unsigned    *next;
    char        buf[80];

#if 0
    if (sdwa->SDWAGR13 < 0x00002000 || sdwa->SDWAGR13 > 0x00FF0000) {
        wtof("Invalid stack pointer");
        goto quit;
    }
#endif
    n = (unsigned*)sdwa->SDWAGR13;

    for(prev=0, try(get_sa_prev, n, &prev); prev ; prev=0, try(get_sa_prev, n, &prev)) {
        n = prev;
        say("------------------------------------------------");
        try(get_epname, n[4], buf);
        if (buf[0]) say(buf);
        try(get_addr, n, buf);
        regline("DSA", (unsigned)n, buf);
        try(get_addr, n[3], buf);
        regline("R14", (unsigned)n[3], buf);
        try(get_addr, n[4], buf);
        regline("R15", (unsigned)n[4], buf);
        try(get_addr, n[5], buf);
        regline("R00", (unsigned)n[5], buf);
        try(get_addr, n[6], buf);
        regline("R01", (unsigned)n[6], buf);
        if (n == ppa) goto quit;        /* stop at A(@PPA) */
    }

    say("------------------------------------------------");
    {
        char    msg[60];
        int     k = 0;

        wtofmt_str(msg, &k, sizeof(msg), "Traceback interrupted, Forward from PPA ");
        wtofmt_hex(msg, &k, sizeof(msg), (unsigned)ppa, 8);
        wto(msg);
    }
    say("------------------------------------------------");

    for(next=ppa; next ; next=0, try(get_sa_next, n, &next)) {
        n = next;
        say("------------------------------------------------");
        try(get_epname, n[4], buf);
        if (buf[0]) say(buf);
        try(get_addr, n, buf);
        regline("DSA", (unsigned)n, buf);
        try(get_addr, n[3], buf);
        regline("R14", (unsigned)n[3], buf);
        try(get_addr, n[4], buf);
        regline("R15", (unsigned)n[4], buf);
        try(get_addr, n[5], buf);
        regline("R00", (unsigned)n[5], buf);
        try(get_addr, n[6], buf);
        regline("R01", (unsigned)n[6], buf);
    }

quit:
    return 0;
}

__asm__("\n&FUNC    SETC 'suppress_dump'");
static int
suppress_dump(SDWA *sdwa)
{
    say("suppress dump requested");
    sdwa->SDWACMPF &= (0XFF - SDWAREQ); /* turn off dump flag */
    return 0;
}

__asm__("\n&FUNC    SETC 'snap_dump'");
static int
snap_dump(SDWA *sdwa)
{
    int     rc;

    say("snap dump requested");

    /* open SNAP DCB for output */
    __asm__( "OPEN  (SNAPDCB,(OUTPUT))         Open SNAP DCB\n\t"
"         LR\t%0,15" : "=r" (rc) : : "0", "1", "14", "15", "memory");
    if (rc) {
        char    msg[48];
        int     k = 0;

        wtofmt_str(msg, &k, sizeof(msg), "OPEN for SNAP DD failed, rc=");
        if (rc < 0) wtofmt_str(msg, &k, sizeof(msg), "-");
        wtofmt_dec(msg, &k, sizeof(msg), (unsigned)(rc < 0 ? -rc : rc));
        wto(msg);
        goto quit;
    }

    __asm__( "LA\t2,SNAPDCB\n\tSNAP DCB=(2),SDATA=ALL,PDATA=ALL"
    : : : "0", "1", "2", "14", "15", "memory");

    __asm__( "CLOSE SNAPDCB                    Close the SNAP DCB" : : : "0", "1", "14", "15", "memory");

    sdwa->SDWACMPF &= (0XFF - SDWAREQ); /* turn off dump flag */

quit:
    return 0;
}

__asm__("\n"
"SNAPDCB  DCB   DDNAME=SNAP,DSORG=PS,LRECL=125,BLKSIZE=1632,            @\n"
"               RECFM=VBA,MACRF=(W)");

__asm__("\n&FUNC    SETC 'system_dump'");
static int
system_dump(SDWA *sdwa)
{
    say("system_dump requested");
    goto quit;

    sdwa->SDWACMPF &= (0XFF - SDWAREQ); /* turn off dump flag */

quit:
    return 0;
}

__asm__("\n&FUNC    SETC 'recovery'");
static int
recovery(SDWA *sdwa, void *udata)
{
    PARAM       *param  = (PARAM*)udata;
    int         dump    = DUMP_DEFAULT;
    unsigned    *psa    = 0;                        /* low core == PSA      */
    void        *tcb    = (void*)psa[0x21c/4];      /* TCB      == PSATOLD  */
    char        *p;
    int         i;
    unsigned    abcode;
    unsigned    epa;
    unsigned    ilc;
    unsigned    cc;
    unsigned    psw[2];
    char        epname[12];
    char        abend[24];
    char        buf[80];

    /* Check SDWACLUP first.  When it is on, RTM entered this exit only to
     * clean up while the task terminates: no retry, and a full dump is
     * pointless.  Crucially, under a terminating TCB this task's libc370
     * CRT is often already gone, so the register/save-area walk below
     * (each probe goes through try() -> __crtget()) would fault or spew
     * "CRT ... not found" for every line.  Emit a single WTO from a
     * pre-formatted buffer and continue with termination.  wto() is
     * CRT-free (SVC 35); do no other C-runtime work on this path.
     */
    if (sdwa->SDWAERRD & SDWACLUP) {
        char    msg[] = "libc370 recovery: abend during task termination, cleanup only";

        wto(msg);
        SETRP(sdwa, 0, 0, 0);   /* RC=0, continue with termination */
        return 0;
    }

#if 0
    wtof("Enter recovery(), dump option=%d", dump);
#endif

    if (param && param->u[1]) {
        dump = (int)param->u[1];
#if 0
        wtof("dump option from SDWAPARM %08X=%d", param, dump);
#endif
    }

    abcode = *(unsigned*)&sdwa->SDWACMPF;
    if (abcode & 0x00FFF000) {
        /* system abend */
        abcode = (abcode & 0x00FFF000) >> 12;
        {
            int k = 0;

            wtofmt_str(abend, &k, sizeof(abend), "S");
            wtofmt_hex(abend, &k, sizeof(abend), abcode, 3);
        }
    }
    else {
        abcode = (abcode & 0x00000FFF);
        {
            int k = 0;

            wtofmt_str(abend, &k, sizeof(abend), "U");
            if (abcode < 1000) wtofmt_str(abend, &k, sizeof(abend), "0");
            if (abcode < 100)  wtofmt_str(abend, &k, sizeof(abend), "0");
            if (abcode < 10)   wtofmt_str(abend, &k, sizeof(abend), "0");
            wtofmt_dec(abend, &k, sizeof(abend), abcode);
        }
    }

    __asm__("MVC\t0(8,%0),0(%1)" : : "r" (psw), "r" (&sdwa->SDWAEMK1) : "memory");
    epa = sdwa->SDWAEPA;
    if (sdwa->SDWANAME[0] <= ' ') {
        int k = 0;

        wtofmt_hex(epname, &k, sizeof(epname), (unsigned)sdwa->SDWAEPA, 8);
    }
    else {
        memcpy(epname, sdwa->SDWANAME, 8);
        epname[8] = 0;
        for(i=0; i < 8; i++) {
            if (epname[i]==' ') {
                epname[i]=0;
                break;
            }
        }
    }

    try(get_offset, sdwa, buf);
    {
        char    msg[126];
        int     k = 0;

        wtofmt_str(msg, &k, sizeof(msg), "ABEND ");
        wtofmt_str(msg, &k, sizeof(msg), abend);
        wtofmt_str(msg, &k, sizeof(msg), " detected for module ");
        wtofmt_str(msg, &k, sizeof(msg), epname);
        wtofmt_str(msg, &k, sizeof(msg), " ");
        wtofmt_str(msg, &k, sizeof(msg), buf);
        wtofmt_str(msg, &k, sizeof(msg), " TCB=");
        wtofmt_hex(msg, &k, sizeof(msg), (unsigned)tcb, 8);
        wto(msg);
    }

    ilc = (sdwa->SDWAPMKP & SDWAILP) >> 5;
    cc  = (sdwa->SDWAINT1 & SDWACC1) >> 4;
    {
        char    msg[80];
        int     k = 0;

        wtofmt_str(msg, &k, sizeof(msg), "PSW:");
        wtofmt_hex(msg, &k, sizeof(msg), psw[0], 8);
        wtofmt_str(msg, &k, sizeof(msg), " ");
        wtofmt_hex(msg, &k, sizeof(msg), psw[1], 8);
        wtofmt_str(msg, &k, sizeof(msg), " KEY(");
        wtofmt_dec(msg, &k, sizeof(msg), (unsigned)((sdwa->SDWAMWP1 & SDWAKEY1) >> 4));
        wtofmt_str(msg, &k, sizeof(msg), ") MODE(");
        wtofmt_str(msg, &k, sizeof(msg), (sdwa->SDWAMWP1 & SDWAPGM1) ? "PROB" : "SUP");
        wtofmt_str(msg, &k, sizeof(msg), ") ILC(");
        wtofmt_dec(msg, &k, sizeof(msg), ilc);
        wtofmt_str(msg, &k, sizeof(msg), ") CC(");
        wtofmt_dec(msg, &k, sizeof(msg), cc);
        wtofmt_str(msg, &k, sizeof(msg), ")");
        wto(msg);
    }

    psw[1] &= 0x00FFFFFF;           /* remove high byte noise */
    if (psw[1] > ilc) psw[1]-=ilc;  /* back up instruction length */
    try(get_addr, psw[1], buf);
    regline(">>>", psw[1], buf);

    /* Guard the register/save-area walk against a missing CRT.  Even with
     * SDWACLUP off, a late fault can arrive after this task's runtime was
     * torn down; the walk probes storage through try() -> __crtget(),
     * which would then fault or log "CRT ... not found" for every probe.
     * Skip the walk rather than emit that per-probe spam.
     */
    if (__crtget()) {
        dump_regs(sdwa);
        dump_sa(sdwa);
    }
    else {
        char    msg[] = "libc370 recovery: CRT unavailable, register/traceback dump skipped";

        wto(msg);
    }

    switch(dump) {
    case DUMP_SUPPRESS:
        suppress_dump(sdwa);
        break;
    case DUMP_DEFAULT:
        /* nothing to do */
        break;
    case DUMP_SNAP:
        snap_dump(sdwa);
        break;
    case DUMP_SDUMP:
        system_dump(sdwa);
        break;
    }

    /* set RC=0, RETRY=NULL, RETREGS=NO */
    SETRP(sdwa,0,0,0);

#if 0
    wtof("Exit recovery()");
#endif
    return 0;   /* 0=continue with abend, 4=continue with retry */
}

int
__abrpt(ESTAE_OP estae_op, DUMP_OP dump_op)
{
    int         rc      = -1;
    int         dump    = (int)dump_op;

    switch(estae_op) {
    case ESTAE_CREATE:
    case ESTAE_OVERLAY:
    case ESTAE_DELETE:
        rc = __estae(estae_op, recovery, (void*)dump);
        break;
    }

    return rc;
}
