/*
 * tstemptl.c - libc370 #199: an empty line on a text stream is a record.
 *
 * ISSUE #199: fputs("a\n\nb\n") on a text stream wrote two records, "a" and
 * "b".  Measured on mvsdev JOB00495 (test/mvs/tstwrpos.c) on FB 80 and VB 84.
 * In text mode __fputc() hands every '\n' to __fflush(), and __fflush()
 * returns at once on an empty buffer - right for fflush()/fclose(), which
 * must not write anything when nothing is pending, and wrong for a newline,
 * which asks for a record whether or not it has data.
 *
 * This test compiles the REAL @@fflush.c and @@fputc.c against a shim for
 * __awrite() that records the length and the bytes of every record handed
 * to the access method.
 *
 * CHECKS
 *   (1) FB text "a\n\nb\n"  -> 3 records, the middle one 80 blanks     RED
 *   (2) VB text "a\n\nb\n"  -> 3 records, the middle one RDW only (4)  RED
 *   (3) U  text "a\n\nb\n"  -> 3 blocks, the middle one a single blank RED
 *   (4) FB text "\n" as the first byte -> 1 blank record               RED
 *   (5) __fflush() on an empty buffer still writes nothing   regression
 *   (6) "a\n" followed by __fflush() -> 1 record, not 2      regression
 *   (7) binary FB: '\n' is data, not a record delimiter      regression
 *
 * BUILD / RUN (host, from test/host; same flag recipe as tsterrfl.c):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R/src/internal -o t tstemptl.c && ./t
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
/* string.h suppressed for the host, as in tsterrfl.c */
#define STRING_H
#include <fileio.h>
#include <stddef.h>
void   *memset(void *s, int c, size_t n);
void   *memcpy(void *t, const void *s, size_t n);
size_t  strlen(const char *s);
int     strcmp(const char *a, const char *b);
char   *strcpy(char *t, const char *s);
void   *memchr(const void *s, int c, size_t n);
int     memcmp(const void *a, const void *b, size_t n);

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ibm/mvs/dcbd.h>

static int  errno_cell;
int *__errno(void) { return &errno_cell; }

static int  bad = 0;

static void check(int cond, const char *what)
{
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* ---- __awrite() shim: records every record handed to it --------------- */

#define MAXREC  8
static int              nrec;
static size_t           reclen[MAXREC];
static unsigned char    recdat[MAXREC][128];

int __awrite(void *handle, unsigned char **buf, size_t *sz)
{
    (void)handle;
    if (nrec < MAXREC) {
        reclen[nrec] = *sz;
        memcpy(recdat[nrec], *buf, *sz <= 128 ? *sz : 128);
    }
    nrec++;
    return 0;
}


/* __fpswt() turns a '+' stream round (#189).  Plain streams never reach
   it: fopen() gives a writer _FILE_FLAG_DCBOUT from the start, and the
   FILEs below are built the same way.  A call here is a test failure. */
static int fpswt_calls;
int __fpswt(FILE *fp, int out)
{
    (void)fp; (void)out;
    fpswt_calls++;
    printf("  !! __fpswt() reached from a plain stream\n");
    errno = EBADF;
    return -1;
}
int __fpupc(FILE *fp, int c)
{
    (void)fp; (void)c;
    fpswt_calls++;
    printf("  !! __fpupc() reached from a plain stream\n");
    errno = EBADF;
    return -1;
}

/* ---- the real thing ---------------------------------------------------- */

#include "../../src/clib/@@fflush.c"
#include "../../src/clib/@@fputc.c"

/* ---- a FILE the shim can serve ----------------------------------------- */

static DCB              fake_dcb;
static unsigned char    filebuf[128];
static unsigned char    asmbuf[128];
static FILE             f;

static void mkfile(unsigned short flags, unsigned char recfm,
                   unsigned lrecl, unsigned bufsize)
{
    memset(&f, 0, sizeof(f));
    memcpy(f.eye, _FILE_EYE, sizeof(f.eye) - 1);
    memset(&fake_dcb, 0, sizeof(fake_dcb));
    fake_dcb.dcbdevt = 0x20;            /* anything but DCBDVTRM           */
    f.dcb     = &fake_dcb;
    f.asmbuf  = asmbuf;
    f.buf     = filebuf;
    f.upto    = filebuf;
    f.endbuf  = filebuf + bufsize;
    f.lrecl   = lrecl;
    f.blksize = 800;
    f.recfm   = recfm;
    f.ungetch = -1;
    f.flags   = flags;
    nrec      = 0;
    memset(reclen, 0, sizeof(reclen));
    memset(recdat, 0, sizeof(recdat));
}

static void put(const char *s)
{
    while (*s) __fputc((unsigned char)*s++, &f);
}

static int allblank(const unsigned char *p, size_t n)
{
    while (n--) if (*p++ != ' ') return 0;
    return 1;
}

#define TXT (_FILE_FLAG_OPEN | _FILE_FLAG_WRITE | _FILE_FLAG_DCBOUT)
#define BIN (_FILE_FLAG_OPEN | _FILE_FLAG_WRITE | _FILE_FLAG_BINARY \
             | _FILE_FLAG_DCBOUT)

int main(void)
{
    printf("=== tstemptl: #199 - an empty line is a record ===\n\n");

    /* (1) FB 80 */
    mkfile(TXT, _FILE_RECFM_F, 80, 80);
    put("a\n\nb\n");
    printf("  FB: %d records\n", nrec);
    check(nrec == 3
          && reclen[0] == 80 && recdat[0][0] == 'a'
          && reclen[1] == 80 && allblank(recdat[1], 80)
          && reclen[2] == 80 && recdat[2][0] == 'b',
          "(1) FB text \"a\\n\\nb\\n\" -> a, 80 blanks, b");

    /* (2) VB 84: buffer is lrecl-4, as __fpopen() sizes it */
    mkfile(TXT, _FILE_RECFM_V, 84, 80);
    put("a\n\nb\n");
    printf("  VB: %d records\n", nrec);
    check(nrec == 3
          && reclen[0] == 5 && recdat[0][4] == 'a'
          && reclen[1] == 4 && recdat[1][0] == 0 && recdat[1][1] == 4
          && reclen[2] == 5 && recdat[2][4] == 'b',
          "(2) VB text \"a\\n\\nb\\n\" -> a, RDW-only record, b");

    /* (3) U: a zero-length block cannot be written */
    mkfile(TXT, _FILE_RECFM_U, 0, 80);
    put("a\n\nb\n");
    printf("  U:  %d blocks\n", nrec);
    check(nrec == 3
          && reclen[0] == 1 && recdat[0][0] == 'a'
          && reclen[1] == 1 && recdat[1][0] == ' '
          && reclen[2] == 1 && recdat[2][0] == 'b',
          "(3) U text \"a\\n\\nb\\n\" -> a, one blank, b");

    /* (4) an empty line as the very first thing written */
    mkfile(TXT, _FILE_RECFM_F, 80, 80);
    put("\n");
    check(nrec == 1 && reclen[0] == 80 && allblank(recdat[0], 80),
          "(4) FB text \"\\n\" first -> one blank record");

    /* (5) fflush()/fclose() with nothing pending write nothing */
    mkfile(TXT, _FILE_RECFM_F, 80, 80);
    __fflush(&f);
    check(nrec == 0, "(5) __fflush() on an empty buffer writes nothing");

    /* (6) a completed line, then a flush: no second, empty record */
    mkfile(TXT, _FILE_RECFM_F, 80, 80);
    put("a\n");
    __fflush(&f);
    check(nrec == 1, "(6) \"a\\n\" + __fflush() -> exactly 1 record");

    /* (7) binary: '\n' is a data byte */
    mkfile(BIN, _FILE_RECFM_F, 80, 80);
    put("a\n\nb");
    __fflush(&f);
    check(nrec == 1 && reclen[0] == 80
          && memcmp(recdat[0], "a\n\nb", 4) == 0 && recdat[0][4] == 0,
          "(7) binary FB: \"a\\n\\nb\" is one record of data");

    printf("\n%s (%d failed)\n", bad ? "RED" : "GREEN", bad);
    return bad ? 1 : 0;
}
