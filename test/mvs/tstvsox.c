/*
 * tstvsox.c - libc370 #623 on MVS: __vsopenx() opens a VSAM cluster with
 * buffers, a password and the MACRF options RST, SKP, SIS and AIX, or
 * with the type left to OPEN; __vsinfo() reads the cluster's attributes.
 *
 * ISSUE #623: vsopen() takes a type, an access and a mode and nothing
 * else.  A cluster with a password could not be opened, a REUSE cluster
 * not emptied, buffers not set, an alternate index not read as itself,
 * and a caller had to know the type before OPEN.
 *
 * jcl/tstvsox.jcl defines a user catalog of its own (with a master
 * password, so that VSAM checks the cluster passwords) and in it:
 *   DD VSAMKS   KSDS KEYS(8 0) RECORDSIZE(80 80) CISZ 4096, KEY00001..5
 *   DD VSAMES   ESDS RECORDSIZE(80 100) CISZ 2048, three records
 *   DD VSAMRR   RRDS RECORDSIZE(80 80) CISZ 1024, three records
 *   DD VSAMRU   KSDS KEYS(4 2) REUSE, three records AA0001..AA0003
 *   DD VSAMPW   KSDS with READPW(RDPW) ATTEMPTS(0), KEY00001..5
 *   DD VSAMPA   the path of an AIX KEYS(6 10) UNIQUEKEY over a KSDS
 *               KEYS(8 0) of three records (ALT003, ALT001, ALT002)
 *
 * Built twice from this source.  TSTVSOXG (-DNEWAPI) uses __vsopenx()
 * and __vsinfo() and links this tree's libc.a.  TSTVSOXR, the red
 * control, links the library of release 2.24.1, which has neither (the
 * NEWAPI build does not link against it: @@VSOPNX and @@VSINFO are
 * unresolved); it checks what vsopen() does with the same requests.
 *
 * Build (S a scratch directory, R a build of v2.24.1):
 *          make
 *          cc370 -O1 -Wall -DNEWAPI -Iinclude -L build/sdk \
 *                test/mvs/tstvsox.c -o $S/TSTVSOXG -flinker-output=iebcopy
 *          cc370 -O1 -Wall -I$R/include -L $R/build/sdk \
 *                test/mvs/tstvsox.c -o $S/TSTVSOXR -flinker-output=iebcopy
 *          cd $S && ld370 --pack TSTVSOXG=TSTVSOXG.iebcopy \
 *                TSTVSOXR=TSTVSOXR.iebcopy -o vsox -xmit \
 *                --dsn IBMUSER.LIBC370.VSOXSCR
 * Install: jcl/recvvsox.jcl.   Run: jcl/tstvsox.jcl.
 *
 * Own MVS/CE container, 2026-10-20 (RECEIVE JOB00021, run JOB00022):
 * GREEN CC 0000, 40/40; RED CC 0001, 1/3 (vsopen() refuses type 3 with
 * EVSTYPE, and the REUSE cluster kept its records).  The same job ran
 * twice before in that container (JOB00019, JOB00020), 40/40 each time:
 * its last step leaves nothing behind.  Measured on the way:
 *   - SHOWCB ACB gives BUFND, BUFNI and BUFSP as the ACB holds them, 0
 *     when none was set; VSAM's defaults are not filled in.
 *   - OPEN takes MACRF=(KEY,ADR) for a KSDS, an ESDS, an RRDS and a
 *     path alike.  vsopen() has always opened an ESDS so: MODCB
 *     MACRF=ADR leaves the model ACB's KEY on.
 *   - RST on a cluster without REUSE: ACBERFLG X'E8'.  RST with MACRF=IN
 *     empties a REUSE cluster too; the next OPEN for input fails
 *     (IEC161I 072).  MACRF=AIX on a DD that names a cluster is ignored.
 *   - Every entry DEFINE created was RACF-indicated (RACF (YES)): RAKF
 *     gives the job's ACEE automatic data set protection.  VSAM then
 *     leaves the decision to RAKF, and the password cluster opens with
 *     the right password, a wrong one and none.  Case (d) checks the
 *     open with the password; a refusal could not be measured here.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <mvs/vsam.h>
#include <mvs/wto.h>

static int run = 0, failed = 0;

static void check(int cond, const char *what)
{
    run++;
    if (!cond) failed++;
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) wtof("TSTVSOX FAIL: %s", what);
}

/* read the next record (key NULL) or the one with key; length or -1/-2 */
static int get(VSFILE *vs, char *buf, void *key, int keylen)
{
    int r;

    memset(buf, 0, 81);
    r = vsread(vs, buf, 80, key, keylen);
    printf("    read -> %d '%.30s'\n", r, r > 0 ? buf : "");
    return r;
}

#ifdef NEWAPI
static VSFILE *openx(const char *dd, VSTYPE t, VSACCESS a, VSMODE m,
                     unsigned flags, const char *pw, int *rc)
{
    VSOPTS  o;
    VSFILE  *vs = NULL;

    memset(&o, 0, sizeof(o));
    o.type      = t;
    o.access    = a;
    o.mode      = m;
    o.flags     = flags;
    o.password  = pw;
    *rc = __vsopenx(dd, &o, &vs);
    printf("    __vsopenx(%s, type %d, flags X'%02X') -> %d%s\n", dd,
           (int)t, flags, *rc, vs ? "" : " (no handle)");
    return vs;
}

static void info(VSFILE *vs, VSINFO *in)
{
    int rc;

    memset(in, 0xFF, sizeof(*in));
    rc = __vsinfo(vs, in);
    printf("    __vsinfo -> %d: type %d keylen %u rkp %u lrecl %u cisz %u"
           " bufnd %u bufni %u bufsp %u\n", rc, (int)in->type, in->keylen,
           in->rkp, in->lrecl, in->cisz, in->bufnd, in->bufni, in->bufsp);
}

/* (a) the type left to OPEN, and the attributes of each type */
static void any(void)
{
    VSFILE  *vs;
    VSINFO  in;
    char    buf[81];
    int     rc;
    unsigned rrn = 2;

    printf(" (a) VSTYPE_ANY on a KSDS, an ESDS and an RRDS\n");
    vs = openx("VSAMKS", VSTYPE_ANY, VSACCESS_DYNAM, VSMODE_IN, 0, NULL, &rc);
    check(rc == 0 && vs, "(a) KSDS opens with VSTYPE_ANY");
    if (vs) {
        info(vs, &in);
        check(in.type == VSTYPE_KSDS && in.keylen == 8 && in.rkp == 0 &&
              in.lrecl == 80 && in.cisz == 4096,
              "(a) KSDS: type KSDS, keylen 8, rkp 0, lrecl 80, cisz 4096");
        check(vs->type == VSTYPE_KSDS, "(a) KSDS: handle type KSDS");
        check(get(vs, buf, "KEY00003", 8) == 80 &&
              memcmp(buf, "KEY00003 THIRD", 14) == 0,
              "(a) KSDS: read by key KEY00003");
        vsclose(vs);
    }

    vs = openx("VSAMES", VSTYPE_ANY, VSACCESS_SEQ, VSMODE_IN, 0, NULL, &rc);
    check(rc == 0 && vs, "(a) ESDS opens with VSTYPE_ANY");
    if (vs) {
        info(vs, &in);
        check(in.type == VSTYPE_ESDS && in.keylen == 0 && in.lrecl == 100 &&
              in.cisz == 2048,
              "(a) ESDS: type ESDS, keylen 0, lrecl 100, cisz 2048");
        check(vs->type == VSTYPE_ESDS, "(a) ESDS: handle type ESDS");
        check(get(vs, buf, NULL, 0) == 80 &&
              memcmp(buf, "ESDS RECORD 1", 13) == 0 &&
              get(vs, buf, NULL, 0) == 80 &&
              memcmp(buf, "ESDS RECORD 2", 13) == 0,
              "(a) ESDS: records 1 and 2 in turn");
        vsclose(vs);
    }

    vs = openx("VSAMRR", VSTYPE_ANY, VSACCESS_DYNAM, VSMODE_IN, 0, NULL, &rc);
    check(rc == 0 && vs, "(a) RRDS opens with VSTYPE_ANY");
    if (vs) {
        info(vs, &in);
        check(in.type == VSTYPE_RRDS && in.lrecl == 80 && in.cisz == 1024,
              "(a) RRDS: type RRDS, lrecl 80, cisz 1024");
        check(vs->type == VSTYPE_RRDS, "(a) RRDS: handle type RRDS");
        check(get(vs, buf, &rrn, 4) == 80 &&
              memcmp(buf, "RRDS RECORD 2", 13) == 0,
              "(a) RRDS: read relative record 2");
        vsclose(vs);
    }
}

/* (b) RST empties a REUSE cluster */
static void rst(void)
{
    VSFILE  *vs;
    char    buf[81];
    int     rc;
    int     n = 0;

    printf(" (b) RST on the REUSE cluster\n");
    vs = openx("VSAMRU", VSTYPE_KSDS, VSACCESS_SEQ, VSMODE_IN, 0, NULL, &rc);
    if (vs) {
        while (get(vs, buf, NULL, 0) > 0) n++;
        vsclose(vs);
    }
    check(rc == 0 && n >= 3, "(b) three records or more before");

    vs = openx("VSAMRU", VSTYPE_KSDS, VSACCESS_SEQ, VSMODE_OUT, VSOPT_RST,
               NULL, &rc);
    check(rc == 0 && vs, "(b) opens for output with VSOPT_RST");
    if (vs) {
        memset(buf, ' ', 80);
        memcpy(buf, "AA0009 AFTER RESET", 18);
        check(vswrite(vs, buf, 80, NULL, 0) == 0, "(b) one record written");
        vsclose(vs);
    }

    n = 0;
    vs = openx("VSAMRU", VSTYPE_KSDS, VSACCESS_SEQ, VSMODE_IN, 0, NULL, &rc);
    if (vs) {
        if (get(vs, buf, NULL, 0) == 80 &&
            memcmp(buf, "AA0009 AFTER RESET", 18) == 0) n++;
        if (get(vs, buf, NULL, 0) == -1) n++;
        vsclose(vs);
    }
    check(rc == 0 && n == 2, "(b) after the reset: only the new record");
}

/* (c) buffers in effect after OPEN */
static void bufs(void)
{
    VSOPTS  o;
    VSFILE  *vs = NULL;
    VSINFO  in;
    int     rc;

    printf(" (c) BUFND, BUFNI and BUFSP\n");
    vs = openx("VSAMKS", VSTYPE_KSDS, VSACCESS_DYNAM, VSMODE_IN, 0, NULL, &rc);
    if (vs) {
        info(vs, &in);
        vsclose(vs);
    }
    check(rc == 0 && in.bufnd == 0 && in.bufni == 0 && in.bufsp == 0,
          "(c) none given: BUFND, BUFNI and BUFSP show 0");

    memset(&o, 0, sizeof(o));
    o.type      = VSTYPE_KSDS;
    o.access    = VSACCESS_DYNAM;
    o.mode      = VSMODE_IN;
    o.bufnd     = 6;
    o.bufni     = 4;
    rc = __vsopenx("VSAMKS", &o, &vs);
    printf("    __vsopenx(BUFND 6, BUFNI 4) -> %d\n", rc);
    if (vs) {
        info(vs, &in);
        vsclose(vs);
    }
    check(rc == 0 && in.bufnd == 6 && in.bufni == 4,
          "(c) BUFND 6, BUFNI 4 after OPEN (SHOWCB)");

    o.bufnd     = 0;
    o.bufni     = 0;
    o.bufsp     = 32768;
    rc = __vsopenx("VSAMKS", &o, &vs);
    printf("    __vsopenx(BUFSP 32768) -> %d\n", rc);
    if (vs) {
        info(vs, &in);
        vsclose(vs);
    }
    check(rc == 0 && in.bufsp == 32768, "(c) BUFSP 32768 after OPEN (SHOWCB)");
}

/* (d) a cluster with a read password */
static void pw(void)
{
    VSFILE  *vs;
    char    buf[81];
    int     rc;

    printf(" (d) the password-protected cluster\n");
    /* The cluster is RACF-indicated (RACF (YES) in LISTCAT): RAKF sets
       automatic data set protection for every user, so DEFINE marks
       every entry, and VSAM asks RACF instead of checking the password.
       Without a password and with a wrong one it opens too; printed,
       not checked. */
    vs = openx("VSAMPW", VSTYPE_KSDS, VSACCESS_SEQ, VSMODE_IN, 0, NULL, &rc);
    if (vs) vsclose(vs);
    vs = openx("VSAMPW", VSTYPE_KSDS, VSACCESS_SEQ, VSMODE_IN, 0, "XXXX",
               &rc);
    if (vs) vsclose(vs);
    vs = openx("VSAMPW", VSTYPE_KSDS, VSACCESS_SEQ, VSMODE_IN, 0, "RDPW",
               &rc);
    check(rc == 0 && vs, "(d) opens with the read password");
    if (vs) {
        check(vs->acb.acbpassw == NULL, "(d) ACB keeps no password address");
        check(get(vs, buf, NULL, 0) == 80 &&
              memcmp(buf, "KEY00001 FIRST", 14) == 0,
              "(d) first record read");
        vsclose(vs);
    }
}

/* (e) skip sequential: a key on each read, forward */
static void skp(void)
{
    VSFILE  *vs;
    char    buf[81];
    int     rc;

    printf(" (e) SKP\n");
    vs = openx("VSAMKS", VSTYPE_KSDS, VSACCESS_SEQ, VSMODE_IN, VSOPT_SKP,
               NULL, &rc);
    check(rc == 0 && vs, "(e) opens with VSOPT_SKP");
    if (vs) {
        check((vs->acb.acbmacr2 & ACBSKP) && (vs->rpl.rplopt1 & RPLSKP),
              "(e) MACRF=SKP and OPTCD=SKP");
        check(get(vs, buf, "KEY00002", 8) == 80 &&
              memcmp(buf, "KEY00002 SECOND", 15) == 0,
              "(e) read KEY00002");
        check(get(vs, buf, "KEY00004", 8) == 80 &&
              memcmp(buf, "KEY00004 FOURTH", 15) == 0,
              "(e) skip to KEY00004");
        vsclose(vs);
    }
}

/* (f) sequential insert strategy */
static void sis(void)
{
    VSFILE  *vs;
    char    buf[81];
    char    rec[80];
    int     rc;

    printf(" (f) SIS\n");
    vs = openx("VSAMKS", VSTYPE_KSDS, VSACCESS_DYNAM, VSMODE_UPD, VSOPT_SIS,
               NULL, &rc);
    check(rc == 0 && vs, "(f) opens with VSOPT_SIS");
    if (vs) {
        check(vs->acb.acbmacr3 & ACBSIS, "(f) MACRF=SIS");
        memset(rec, ' ', 80);
        memcpy(rec, "KEY00006 INSERTED", 17);
        check(vswrite(vs, rec, 80, rec, 8) == 0, "(f) KEY00006 inserted");
        check(get(vs, buf, "KEY00006", 8) == 80 &&
              memcmp(buf, "KEY00006 INSERTED", 17) == 0,
              "(f) KEY00006 read back");
        vsclose(vs);
    }
}

/* (g) the alternate index through its path */
static void aix(void)
{
    VSFILE  *vs;
    VSINFO  in;
    char    buf[81];
    int     rc;

    printf(" (g) AIX through the path DD\n");
    vs = openx("VSAMPA", VSTYPE_KSDS, VSACCESS_SEQ, VSMODE_IN, 0, NULL, &rc);
    check(rc == 0 && vs, "(g) the path opens without VSOPT_AIX");
    if (vs) {
        check(get(vs, buf, NULL, 0) == 80 &&
              memcmp(buf, "KEY00002  ALT001", 16) == 0,
              "(g) path: the base record of ALT001 first");
        vsclose(vs);
    }
    vs = openx("VSAMPA", VSTYPE_ANY, VSACCESS_SEQ, VSMODE_IN, VSOPT_AIX,
               NULL, &rc);
    check(rc == 0 && vs, "(g) the path opens with VSOPT_AIX");
    if (vs) {
        info(vs, &in);
        check(in.type == VSTYPE_KSDS && in.keylen == 6 && in.rkp == 5,
              "(g) AIX: KSDS, keylen 6, rkp 5");
        rc = get(vs, buf, NULL, 0);
        check(rc == 19 && memcmp(buf + 5, "ALT001KEY00002", 14) == 0,
              "(g) AIX: first record 19 bytes, ALT001 -> KEY00002");
        vsclose(vs);
    }
}

/* (h) what is refused before OPEN */
static void refused(void)
{
    VSOPTS  o;
    VSFILE  *vs = (VSFILE *)1;
    int     rc;

    printf(" (h) refusals\n");
    memset(&o, 0, sizeof(o));
    o.type      = VSTYPE_KSDS;
    o.access    = VSACCESS_SEQ;
    o.mode      = VSMODE_IN;
    o.flags     = 0x100;
    rc = __vsopenx("VSAMKS", &o, &vs);
    check(rc == EINVAL && errno == EINVAL && vs == NULL,
          "(h) unknown flag: EINVAL");
    o.flags     = 0;
    o.password  = "PASSWORD9";
    rc = __vsopenx("VSAMKS", &o, &vs);
    check(rc == EINVAL, "(h) password of nine characters: EINVAL");
    o.password  = NULL;
    o.type      = VSTYPE_ESDS;
    o.flags     = VSOPT_SKP;
    rc = __vsopenx("VSAMES", &o, &vs);
    check(rc == EVSACCESS, "(h) SKP on an ESDS: EVSACCESS");
    o.type      = VSTYPE_ANY;
    rc = __vsopenx("VSAMES", &o, &vs);
    check(rc == EVSACCESS && vs == NULL,
          "(h) SKP with VSTYPE_ANY on an ESDS: EVSACCESS after OPEN");
    rc = vsopen("VSAMKS", VSTYPE_ANY, VSACCESS_SEQ, VSMODE_IN, &vs);
    check(rc == EVSTYPE, "(h) vsopen() refuses VSTYPE_ANY: EVSTYPE");
}
#endif

int main(void)
{
#ifdef NEWAPI
    printf("=== tstvsox (GREEN, __vsopenx): VSAM open options (#623) ===\n");
    any();
    rst();
    bufs();
    pw();
    skp();
    sis();
    aix();
    refused();
#else
    VSFILE  *vs = NULL;
    char    buf[81];
    int     rc;
    int     n = 0;

    printf("=== tstvsox (RED, vsopen): VSAM open options (#623) ===\n");
    /* the type cannot be left to OPEN */
    rc = vsopen("VSAMKS", (VSTYPE)3, VSACCESS_SEQ, VSMODE_IN, &vs);
    printf("    vsopen(type 3) -> %d\n", rc);
    check(rc == 0, "(a) a KSDS opens without its type");
    if (vs) vsclose(vs);
    vs = NULL;

    /* a password cannot be given */
    rc = vsopen("VSAMPW", VSTYPE_KSDS, VSACCESS_SEQ, VSMODE_IN, &vs);
    printf("    vsopen(VSAMPW) -> %d\n", rc);
    check(rc == 0, "(d) the password-protected cluster opens");
    if (vs) vsclose(vs);
    vs = NULL;

    /* output does not empty a REUSE cluster: a fourth record is added */
    rc = vsopen("VSAMRU", VSTYPE_KSDS, VSACCESS_SEQ, VSMODE_OUT, &vs);
    if (vs) {
        memset(buf, ' ', 80);
        memcpy(buf, "AA0004 NO RESET", 15);
        printf("    vswrite -> %d\n", vswrite(vs, buf, 80, NULL, 0));
        vsclose(vs);
    }
    vs = NULL;
    rc = vsopen("VSAMRU", VSTYPE_KSDS, VSACCESS_SEQ, VSMODE_IN, &vs);
    if (vs) {
        while (get(vs, buf, NULL, 0) > 0) n++;
        vsclose(vs);
    }
    printf("    VSAMRU holds %d records\n", n);
    check(rc == 0 && n == 1, "(b) the REUSE cluster was emptied");
#endif

    printf("=== tstvsox: %d/%d passed ===\n", run - failed, run);
    wtof("TSTVSOX %s %d/%d", failed ? "RED" : "GREEN", run - failed, run);
    return failed ? 1 : 0;
}
