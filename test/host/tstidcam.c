/*
 * tstidcam.c - libc370 #71: idcams_sysprint() hands every SYSPRINT line,
 * with its IDC message number, to the caller.
 *
 * 1.x's IDCAMS output exit (__idcexc() in src/mvs/idcams/idcams.c) received
 * each SYSPRINT record together with ioflags->msgno -- the IDCnnnn number --
 * and dropped both, so idcams() could only return the condition code, and 8
 * meant "not found" and "refused" alike (ftpd#87).
 *
 * This compiles the REAL idcams.c and drives its exit the way IDCAMS does:
 * OP_OPEN, OP_GET (the commands), several OP_PUT, OP_CLOSE.  It checks:
 *
 *   1. OP_GET hands over the commands once, then end-of-data (rc 4);
 *   2. with a callback, every OP_PUT reaches it in order, the record pointer
 *      and its length untouched, arg passed through, and msgno the IDC
 *      number read from the record -- IDCAMS's own ioflags->msgno drops
 *      the leading digit (IDC3012I arrives as 12) and gives the summaries
 *      -1 and -2, as measured on MVS (JOB01058), so the test hands the
 *      exit exactly those values;
 *   3. without one (plain idcams()), OP_PUT does nothing and returns 0;
 *   4. idcams() and idcams_sysprint() return what __linkds() reports: the
 *      condition code, or a negative value when the LINK failed.
 *
 * What the host cannot show: link_idcams() passes the exit's data to IDCAMS
 * through 31-bit words in its parameter list, which a 64-bit host truncates,
 * so the path through a real IDCAMS is the MVS test's (test/mvs/tstidcsp.c).
 *
 * BUILD / RUN: test/host/run.sh tstidcam
 */
#include <stdio.h>
#include <stdlib.h>

/* libc370's <string.h> carries S/370 inline assembler a host compiler cannot
 * take; idcams.c and s370/savearea.h need strlen() and memcpy(), so stand in
 * for the header */
#define STRING_H
size_t strlen(const char *s);
void *memcpy(void *d, const void *s, size_t n);

#include "../../src/mvs/idcams/idcams.c"

/* the assembler glue and the LINK, which only exist on MVS */
int __idcex(UDATA *udata, IOFLAGS *ioflags, IOINFO *ioinfo)
{ return __idcexc(udata, ioflags, ioinfo); }
static int link_rc, link_prc;
int __linkds(const char *pgm, void *dcb, void *r1, int *prc)
{ (void) pgm; (void) dcb; (void) r1; *prc = link_prc; return link_rc; }

static int fails, checks;
#define CHECK(cond, msg)                                                    \
    do { checks++; if (!(cond)) { fails++; printf("  FAIL %s\n", msg); } } while (0)
#define CHECK_EQ(got, want, msg)                                            \
    do { long g_ = (long) (got), w_ = (long) (want); checks++;              \
         if (g_ != w_) { fails++;                                           \
         printf("  FAIL %s: got %ld, want %ld\n", msg, g_, w_); } } while (0)

/* what the callback saw */
static struct { int msgno; const char *text; int len; void *arg; } seen[8];
static int nseen;
static void capture(void *arg, int msgno, const char *text, int len)
{
    if (nseen < 8) {
        seen[nseen].msgno = msgno; seen[nseen].text = text;
        seen[nseen].len = len;     seen[nseen].arg = arg;
    }
    nseen++;
}

static int op(UDATA *u, int code, IOINFO *info, short msgno)
{
    IOFLAGS f = { 0 };
    f.op = (char) code;
    f.msgno = msgno;
    return __idcexc(u, &f, info);
}

int main(void)
{
    /* records as IDCAMS writes them: a carriage-control byte, then text */
    static const char l1[] = "  DELETE 'IBMUSER.NOSUCHDS'";
    static const char l2[] = "0IDC3012I ENTRY IBMUSER.NOSUCHDS NOT FOUND";
    static const char l3[] = "0IDC0002I IDCAMS PROCESSING COMPLETE. MAXIMUM CONDITION CODE WAS 8";
    static const char l4[] = "\0IDC3009I ** VSAM CATALOG RETURN CODE IS 8";
    static const char l5[] = "0IDCX012I NOT A NUMBER";
    int     token   = 42;
    UDATA   u       = { 0, " DELETE 'IBMUSER.NOSUCHDS'", capture, &token };
    UDATA   plain   = { 0, " LISTCAT", 0, 0 };
    IOINFO  info;

    /* 1. OP_GET: the commands once, then end-of-data */
    CHECK_EQ(op(&u, OP_OPEN, &info, 0), 0, "OP_OPEN rc");
    CHECK_EQ(op(&u, OP_GET, &info, 0), 0, "first OP_GET rc");
    CHECK(info.get.rec != 0 && strlen(info.get.rec) == (size_t) info.get.reclen,
          "first OP_GET hands over the commands");
    CHECK_EQ(op(&u, OP_GET, &info, 0), 4, "second OP_GET is end-of-data");
    CHECK_EQ(info.get.reclen, 0, "second OP_GET has no record");

    /* 2. OP_PUT with a callback: every line, in order, untouched */
    info.put.rec = (char *) l1; info.put.reclen = (int) sizeof l1 - 1;
    CHECK_EQ(op(&u, OP_PUT, &info, 0), 0, "OP_PUT (echo) rc");
    info.put.rec = (char *) l2; info.put.reclen = (int) sizeof l2 - 1;
    CHECK_EQ(op(&u, OP_PUT, &info, 12), 0, "OP_PUT (IDC3012I) rc");
    info.put.rec = (char *) l3; info.put.reclen = (int) sizeof l3 - 1;
    CHECK_EQ(op(&u, OP_PUT, &info, -2), 0, "OP_PUT (IDC0002I) rc");
    info.put.rec = (char *) l4; info.put.reclen = (int) sizeof l4 - 1;
    CHECK_EQ(op(&u, OP_PUT, &info, 9), 0, "OP_PUT (IDC3009I, CC x'00') rc");
    info.put.rec = (char *) l5; info.put.reclen = (int) sizeof l5 - 1;
    CHECK_EQ(op(&u, OP_PUT, &info, 12), 0, "OP_PUT (malformed number) rc");
    CHECK_EQ(op(&u, OP_CLOSE, &info, 0), 0, "OP_CLOSE rc");

    CHECK_EQ(nseen, 5, "the callback saw five lines");
    CHECK_EQ(seen[0].msgno, 0,    "line 1: the echoed command has no number");
    CHECK_EQ(seen[1].msgno, 3012, "line 2: IDC3012I is 3012, not IDCAMS's 12");
    CHECK_EQ(seen[2].msgno, 2,    "line 3: IDC0002I is 2, not IDCAMS's -2");
    CHECK_EQ(seen[3].msgno, 3009, "line 4: IDC3009I behind a x'00' control byte");
    CHECK_EQ(seen[4].msgno, 0,    "line 5: IDCX012I is no number");
    CHECK(seen[0].text == l1 && seen[1].text == l2 && seen[2].text == l3,
          "the record pointers are handed over as they are");
    CHECK_EQ(seen[1].len, (int) sizeof l2 - 1, "line 2: its length");
    CHECK(seen[0].arg == &token && seen[2].arg == &token, "arg is passed through");

    /* 3. plain idcams(): no callback, OP_PUT does nothing */
    nseen = 0;
    info.put.rec = (char *) l2; info.put.reclen = (int) sizeof l2 - 1;
    CHECK_EQ(op(&plain, OP_PUT, &info, 3012), 0, "OP_PUT without callback rc");
    CHECK_EQ(nseen, 0, "OP_PUT without callback calls nothing");

    /* 4. the return value is idcams()'s */
    link_rc = 0; link_prc = 8;
    CHECK_EQ(idcams_sysprint(capture, &token, " DELETE '%s'", "X.Y"), 8,
             "idcams_sysprint() returns the condition code");
    CHECK_EQ(idcams(" DELETE '%s'", "X.Y"), 8, "idcams() unchanged");
    link_rc = 806;
    CHECK_EQ(idcams_sysprint(capture, &token, " LISTCAT"), -806,
             "a failed LINK is negative");

    printf("tstidcam: %d of %d checks passed\n", checks - fails, checks);
    return fails ? 1 : 0;
}
