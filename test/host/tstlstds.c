/*
 * tstlstds.c - libc370 #50: __listds() returns the catalog each entry was
 * found in, and __freeds() frees it exactly once.
 *
 * ISSUE #50 (from crent370#13): struct dslist had no catalog name, so mvsMF
 * answers every z/OSMF data set list with "catnm": "".  The name is in the
 * LISTCAT output __listds() already reads - measured on mvsdev, JOB01086:
 * IDCAMS on MVS 3.8j prints it PER ENTRY, right after the entry line and
 * ahead of its volumes:
 *
 *     NONVSAM ------- IBMUSER.BREXX370.PROBESEQ
 *          IN-CAT --- UCPUB000
 *          HISTORY
 *          ...
 *          VOLUMES
 *            VOLSER------------WORK01     DEVTYPE------X'3050200B'
 *
 * LEVEL('IBMUSER') answered UCPUB000 (the alias's user catalog) for every
 * entry, LEVEL('SYS2') SYS1.VSAM.MASTER.CATALOG.  There is no "LISTING FROM
 * CATALOG" header line.
 *
 * THE SHAPE: `const char *catnm` at the end of struct dslist, not a
 * char[45].  sizeof(DSLIST) is 98 under cc370 and @@GETM rounds every request
 * to (n + 8 + 63) & ~63, so a record costs 128 bytes; a pointer keeps it at
 * 128, an inline name would make it 192.  The string belongs to the list:
 * consecutive entries from one catalog share it, and __freeds() frees it.
 *
 * WHAT THIS TEST PINS - it runs the REAL @@listds.c parse() and @@freeds.c on
 * LISTCAT lines copied from JOB01086, with __listc() shimmed to hand them
 * over, and counts every malloc/calloc/strdup against every free:
 *
 *   (1)  two entries from one catalog -> both carry "UCPUB000", and it is
 *        ONE string (same pointer, one allocation).
 *   (2)  two from UCPUB000, one from the master catalog -> each entry its own
 *        catalog; two strings.
 *   (3)  an entry with no IN-CAT line -> catnm NULL, the record still built.
 *   (4)  carriage control in column 1 ('0' before the entry line, as in the
 *        OUTFILE) and a page header between IN-CAT and VOLSER -> the name
 *        still reaches the record.
 *   (5)  an entry the filter drops -> its catalog name is never kept.
 *   (6)  an entry with an IN-CAT line but no volume, last in the listing ->
 *        no record, and the name nobody took is freed by __listds() itself.
 *   (7)  catalogs alternating, then the array REVERSED before __freeds() -
 *        mvsMF sorts what it gets (dsapi.c) -> every name freed once.  A
 *        double free is ASAN's to report; a missed one is the count's.
 *   (8)  #308: an entry with no VOLSER, then another -> the second is
 *        listed with its own volume; it used to vanish, its volume landing
 *        on the first.
 *   (9)  #308: LISTC LEVEL('SYS1') VOLUME as mvsdev printed it (JOB01341):
 *        page spaces and a cluster without volumes between data sets.
 *   (10) a cluster whose volume comes from its DATA component, with an
 *        indented "CLUSTER--name" association in between (not measured).
 *   after every case: no allocation left once __freeds() has run.
 *
 * Before the fix this does not build: struct dslist has no catnm.
 *
 * WHAT IT DOES NOT PIN: that LISTCAT prints IN-CAT for every option a caller
 * may pass (measured for "NONVSAM VOLUME", what mvsMF and ftpd pass).
 *
 * BUILD AND RUN (host, from test/host)
 *
 *     cc -std=gnu99 -Wall -fsanitize=address \
 *        -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I ../../include -I ../.. -o tstlstds tstlstds.c \
 *        ../../src/ext/strutil/@@patmat.c
 *     ./tstlstds                                      # rc 0 when green
 */
/* string.h suppressed and its functions declared here: its memset() is
 * static __inline S/370 assembler that -D'__asm__(...)=' does not reach, and
 * @@listds.c calls it - see tstfprls.c note (a) */
#define STRING_H
#ifndef __SIZE_T_DEFINED
#define __SIZE_T_DEFINED
typedef unsigned long size_t;
#endif
#ifndef NULL
#define NULL ((void *)0)
#endif
void   *memcpy(void *, const void *, size_t);
void   *memset(void *, int, size_t);
int     memcmp(const void *, const void *, size_t);
char   *strcpy(char *, const char *);
char   *strcat(char *, const char *);
char   *strtok(char *, const char *);
char   *strdup(const char *);
size_t  strlen(const char *);
int     strcmp(const char *, const char *);
int     strcasecmp(const char *, const char *);

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

/* --------------------------------------------------------------------------
 * Allocation counter: every allocation the code under test makes, against
 * every free.  Defined before the TUs are included so their calls land here.
 * ------------------------------------------------------------------------ */
static long live;

static void *tst_calloc(size_t n, size_t s){ void *p = calloc(n, s); if (p) live++; return p; }
static char *tst_strdup(const char *s)     { char *p = strdup(s);    if (p) live++; return p; }
static void  tst_free(void *p)             { if (p) live--; free(p); }

static int strdups;                         /* catalog names allocated */
static char *count_strdup(const char *s)   { strdups++; return tst_strdup(s); }

#define calloc  tst_calloc
#define strdup  count_strdup
#define free    tst_free

#include "../../src/mvs/dslist/@@listds.c"
#include "../../src/mvs/dslist/@@freeds.c"
#include "../../src/ext/array/@@aradd.c"
#include "../../src/ext/array/@@arnew.c"
#include "../../src/ext/array/@@arcou.c"
#include "../../src/ext/array/@@arget.c"
#include "../../src/ext/array/@@arfre.c"

#undef calloc
#undef strdup
#undef free

/* --------------------------------------------------------------------------
 * Shims for the MVS services @@listds.c reaches.
 * ------------------------------------------------------------------------ */
static int the_errno;
int *__errno(void) { return &the_errno; }

/* libc370's ctype.h is table lookups; tables for the host's ASCII */
static unsigned short isbuf_[256];
static short          tolow_[256];
static short          toup_[256];

unsigned short *__isbuf = isbuf_;
short          *__tolow = tolow_;
short          *__toup  = toup_;

static void ctype_init(void)
{
    int c;

    for (c = 0; c < 256; c++) {
        tolow_[c] = (short)((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);
        toup_[c]  = (short)((c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c);
        isbuf_[c] = 0;
        if (c >= '0' && c <= '9') isbuf_[c] |= 0x0008U;   /* isdigit */
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
            isbuf_[c] |= 0x0002U;                         /* isalpha */
        if (isbuf_[c] & 0x000AU) isbuf_[c] |= 0x0001U;    /* isalnum */
    }
}

/* no DSCB: __listds() keeps the record with its name and volume only */
int __dscbdv(const char dsn[44], const char vol[6], DSCB *dscb)
{
    (void)dsn; (void)vol; (void)dscb;
    return 1;
}
int __dscbv(const char vol[6], DSCB *dscb) { (void)vol; (void)dscb; return 1; }
int __locate(const char dsn[44], LOCWORK *w) { (void)dsn; (void)w; return 1; }

/* __listc(): hands the fixture's lines to the callback, as the real one hands
 * it the records of the IDCAMS OUTFILE */
static const char **fx_lines;

int __listc(const char *level, const char *option,
            int (*prt)(void *udata, const char *fmt, ...), void *udata)
{
    const char **l;

    (void)level; (void)option;
    for (l = fx_lines; *l; l++) prt(udata, "%s", *l);
    return 0;
}

/* --------------------------------------------------------------------------
 * Minimal mbtcheck.h-compatible harness.
 * ------------------------------------------------------------------------ */
static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

#define CHECK_EQ(got, want, msg)                                          \
    do {                                                                  \
        long g_ = (long)(got), w_ = (long)(want);                         \
        mbt_run++;                                                        \
        if (g_ == w_) { mbt_passed++; printf("  PASS: %s\n", (msg)); }    \
        else { mbt_failed++;                                              \
               printf("  FAIL: %s (got %ld, want %ld)\n", (msg), g_, w_); }\
    } while (0)

#define CHECK_STR(got, want, msg)                                         \
    do {                                                                  \
        const char *g_ = (got), *w_ = (want);                             \
        int ok_ = (g_ && w_) ? strcmp(g_, w_) == 0 : g_ == w_;            \
        mbt_run++;                                                        \
        if (ok_) { mbt_passed++; printf("  PASS: %s\n", (msg)); }         \
        else { mbt_failed++; printf("  FAIL: %s (got \"%s\", want \"%s\")\n",\
                                   (msg), g_ ? g_ : "(null)",              \
                                   w_ ? w_ : "(null)"); }                 \
    } while (0)

static int mbt_test_summary(const char *name)
{
    printf("\n=== %s: %d/%d passed", name, mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}

/* --------------------------------------------------------------------------
 * Fixtures: LISTCAT lines as JOB01086 printed them.
 * ------------------------------------------------------------------------ */
#define ENTRY(dsn)  "NONVSAM ------- " dsn
#define INCAT(cat)  "     IN-CAT --- " cat
#define HISTORY     "     HISTORY",                                               \
                    "       OWNER-IDENT-------(NULL)     CREATION----------26.270", \
                    "       RELEASE----------------2     EXPIRATION--------00.000"
#define VOLS(vol)   "     VOLUMES",                                               \
                    "       VOLSER------------" vol "     DEVTYPE------X'3050200B'"
#define MASTER      "SYS1.VSAM.MASTER.CATALOG"

static DSLIST **run(const char **lines, const char *filter)
{
    fx_lines = lines;
    strdups  = 0;
    return __listds("IBMUSER", "NONVSAM VOLUME", filter);
}

static void done(DSLIST **list)
{
    __freeds(&list);
    CHECK(list == NULL, "__freeds() cleared the caller's pointer");
    CHECK_EQ(live, 0, "no allocation left after __freeds()");
    live = 0;
}

/* ------------------------------------------------------------------------ */
int main(void)
{
    DSLIST  **l;

    ctype_init();

    printf("=== TSTLSTDS - libc370 #50: the catalog name in DSLIST ===\n");

    /* ---------------------------------------------------------------- */
    printf("\n(1) two entries, one catalog -> one shared string\n");
    {
        static const char *fx[] = {
            "  LISTC LEVEL('IBMUSER') NONVSAM VOLUME",
            ENTRY("IBMUSER.BREXX370.PROBESEQ"), INCAT("UCPUB000"), HISTORY,
            VOLS("WORK01"),
            ENTRY("IBMUSER.BREXX370.RXLIB"), INCAT("UCPUB000"), HISTORY,
            VOLS("WORK01"),
            NULL };
        l = run(fx, NULL);
        CHECK_EQ(arraycount(&l), 2, "two records");
        CHECK_STR(l[0]->dsn, "IBMUSER.BREXX370.PROBESEQ", "record 1 dsn");
        CHECK_STR(l[0]->volser, "WORK01", "record 1 volser");
        CHECK_STR(l[0]->catnm, "UCPUB000", "record 1 catnm");
        CHECK_STR(l[1]->catnm, "UCPUB000", "record 2 catnm");
        CHECK(l[0]->catnm == l[1]->catnm, "one string, shared");
        CHECK_EQ(strdups, 1, "one name allocated");
        done(l);
    }

    /* ---------------------------------------------------------------- */
    printf("\n(2) two catalogs -> each entry its own\n");
    {
        static const char *fx[] = {
            ENTRY("IBMUSER.A"), INCAT("UCPUB000"), HISTORY, VOLS("WORK00"),
            ENTRY("IBMUSER.B"), INCAT("UCPUB000"), HISTORY, VOLS("WORK00"),
            ENTRY("SYS2.CMDLIB"), INCAT(MASTER), HISTORY, VOLS("MVS000"),
            NULL };
        l = run(fx, NULL);
        CHECK_EQ(arraycount(&l), 3, "three records");
        CHECK_STR(l[0]->catnm, "UCPUB000", "record 1 catnm");
        CHECK_STR(l[1]->catnm, "UCPUB000", "record 2 catnm");
        CHECK_STR(l[2]->catnm, MASTER, "record 3 catnm");
        CHECK_STR(l[2]->volser, "MVS000", "record 3 volser");
        CHECK_EQ(strdups, 2, "two names allocated");
        done(l);
    }

    /* ---------------------------------------------------------------- */
    printf("\n(3) no IN-CAT line -> NULL, record still built\n");
    {
        static const char *fx[] = {
            ENTRY("IBMUSER.A"), INCAT("UCPUB000"), HISTORY, VOLS("WORK00"),
            ENTRY("IBMUSER.NOCAT"), HISTORY, VOLS("WORK01"),
            NULL };
        l = run(fx, NULL);
        CHECK_EQ(arraycount(&l), 2, "two records");
        CHECK_STR(l[0]->catnm, "UCPUB000", "record 1 catnm");
        CHECK_STR(l[1]->dsn, "IBMUSER.NOCAT", "record 2 dsn");
        CHECK(l[1]->catnm == NULL, "record 2 catnm NULL - not the one before");
        done(l);
    }

    /* ---------------------------------------------------------------- */
    printf("\n(4) carriage control, and a page header inside the entry\n");
    {
        static const char *fx[] = {
            "1IDCAMS  SYSTEM SERVICES                         TIME: 04:17:08",
            "0NONVSAM ------- IBMUSER.CC",
            " " INCAT("UCPUB000"),
            "1IDCAMS  SYSTEM SERVICES                         TIME: 04:17:08",
            HISTORY, VOLS("WORK00"),
            NULL };
        l = run(fx, NULL);
        CHECK_EQ(arraycount(&l), 1, "one record");
        CHECK_STR(l[0]->dsn, "IBMUSER.CC", "dsn after '0'");
        CHECK_STR(l[0]->catnm, "UCPUB000", "catnm across a page header");
        done(l);
    }

    /* ---------------------------------------------------------------- */
    printf("\n(5) an entry the filter drops keeps no name\n");
    {
        static const char *fx[] = {
            ENTRY("IBMUSER.SKIP.ME"), INCAT("UCSKIP00"), HISTORY,
            VOLS("WORK00"),
            ENTRY("IBMUSER.KEEP.DATA"), INCAT("UCPUB000"), HISTORY,
            VOLS("WORK01"),
            NULL };
        l = run(fx, "IBMUSER.KEEP.*");
        CHECK_EQ(arraycount(&l), 1, "one record");
        CHECK_STR(l[0]->dsn, "IBMUSER.KEEP.DATA", "the kept one");
        CHECK_STR(l[0]->catnm, "UCPUB000", "its catalog");
        CHECK_EQ(strdups, 1, "the dropped entry's name never allocated");
        done(l);
    }

    /* ---------------------------------------------------------------- */
    printf("\n(6) IN-CAT but no volume -> no record, name freed\n");
    /* An entry with no volume in the middle of a listing is case (8). */
    {
        static const char *fx[] = {
            ENTRY("IBMUSER.A"), INCAT("UCPUB000"), HISTORY, VOLS("WORK00"),
            ENTRY("IBMUSER.NOVOL"), INCAT("UCORPHAN"), HISTORY,
            NULL };
        l = run(fx, NULL);
        CHECK_EQ(arraycount(&l), 1, "orphan last: one record");
        CHECK_STR(l[0]->catnm, "UCPUB000", "its catalog");
        done(l);    /* the last name read, taken by nobody */
    }

    /* ---------------------------------------------------------------- */
    printf("\n(7) alternating catalogs, array reversed before __freeds()\n");
    {
        static const char *fx[] = {
            ENTRY("IBMUSER.A"), INCAT("UCPUB000"), HISTORY, VOLS("WORK00"),
            ENTRY("IBMUSER.B"), INCAT("UCPUB000"), HISTORY, VOLS("WORK00"),
            ENTRY("SYS2.X"), INCAT(MASTER), HISTORY, VOLS("MVS000"),
            ENTRY("IBMUSER.C"), INCAT("UCPUB000"), HISTORY, VOLS("WORK01"),
            ENTRY("SYS2.Y"), INCAT(MASTER), HISTORY, VOLS("MVS000"),
            ENTRY("IBMUSER.D"), INCAT("UCPUB000"), HISTORY, VOLS("WORK01"),
            NULL };
        unsigned n, i;

        l = run(fx, NULL);
        n = arraycount(&l);
        CHECK_EQ(n, 6, "six records");
        CHECK_EQ(strdups, 5, "a new string at every change of catalog");
        CHECK(l[0]->catnm == l[1]->catnm, "neighbours share");
        CHECK(l[1]->catnm != l[3]->catnm, "not across another catalog");
        CHECK_STR(l[3]->catnm, "UCPUB000", "record 4 catnm");
        CHECK_STR(l[4]->catnm, MASTER, "record 5 catnm");
        for (i = 0; i < n / 2; i++) {
            DSLIST *t = l[i];
            l[i] = l[n - 1 - i];
            l[n - 1 - i] = t;
        }
        done(l);
    }

    /* ---------------------------------------------------------------- */
    printf("\n(8) #308: an entry without VOLSER does not swallow the next\n");
    {
        static const char *fx[] = {
            ENTRY("IBMUSER.NOVOL"), INCAT("UCPUB000"), HISTORY,
            ENTRY("IBMUSER.A"), INCAT("UCPUB000"), HISTORY, VOLS("WORK00"),
            NULL };
        l = run(fx, NULL);
        CHECK_EQ(arraycount(&l), 1, "one record");
        CHECK_STR(l[0]->dsn, "IBMUSER.A", "the entry after the orphan");
        CHECK_STR(l[0]->volser, "WORK00", "with its own volume");
        CHECK_STR(l[0]->catnm, "UCPUB000", "and its catalog");
        done(l);
    }

    /* ---------------------------------------------------------------- */
    printf("\n(9) #308: LISTC LEVEL('SYS1') VOLUME as mvsdev printed it\n");
    /* JOB01341: page spaces and clusters carry no VOLUMES block.  Before
       the fix SYS1.PAGECSA came back on SYS1.PARMLIB's volume and
       SYS1.PARMLIB was missing (JOB01340, the previous library). */
    {
        static const char *fx[] = {
            "PAGESPACE ----- SYS1.PAGECSA", INCAT(MASTER), HISTORY,
            "PAGESPACE ----- SYS1.PAGELPA", INCAT(MASTER), HISTORY,
            "PAGESPACE ----- SYS1.PAGEL00", INCAT(MASTER), HISTORY,
            "NONVSAM ------- SYS1.PARMLIB", INCAT(MASTER), HISTORY,
            VOLS("MVSRES"),
            "CLUSTER ------- SYS1.STGINDEX", INCAT(MASTER), HISTORY,
            "NONVSAM ------- SYS1.SVCLIB", INCAT(MASTER), HISTORY,
            VOLS("MVSRES"),
            NULL };
        l = run(fx, NULL);
        CHECK_EQ(arraycount(&l), 2, "two records - the two with a volume");
        CHECK_STR(l[0]->dsn, "SYS1.PARMLIB", "record 1 is SYS1.PARMLIB");
        CHECK_STR(l[1]->dsn, "SYS1.SVCLIB", "record 2 is SYS1.SVCLIB");
        CHECK_STR(l[1]->volser, "MVSRES", "record 2 volser");
        done(l);
    }

    /* ---------------------------------------------------------------- */
    printf("\n(10) #308: a cluster still takes its DATA component's VOLSER\n");
    /* The shape the state across lines exists for.  NOT measured: the
       mvsdev catalog lists no component under LEVEL (JOB01341) - this is
       the layout of the IDCAMS manual, kept as the guard that an indented
       "CLUSTER--name" association does not start an entry. */
    {
        static const char *fx[] = {
            "CLUSTER ------- IBMUSER.KSDS", INCAT("UCPUB000"), HISTORY,
            "     ASSOCIATIONS",
            "       DATA-----IBMUSER.KSDS.DATA",
            "   DATA ------- IBMUSER.KSDS.DATA", INCAT("UCPUB000"), HISTORY,
            "     ASSOCIATIONS",
            "       CLUSTER--IBMUSER.KSDS",
            VOLS("WORK00"),
            ENTRY("IBMUSER.A"), INCAT("UCPUB000"), HISTORY, VOLS("WORK01"),
            NULL };
        l = run(fx, NULL);
        CHECK_EQ(arraycount(&l), 2, "two records");
        CHECK_STR(l[0]->dsn, "IBMUSER.KSDS", "record 1 is the cluster");
        CHECK_STR(l[0]->volser, "WORK00", "on its DATA component's volume");
        CHECK_STR(l[1]->dsn, "IBMUSER.A", "record 2");
        done(l);
    }

    return mbt_test_summary("TSTLSTDS");
}
