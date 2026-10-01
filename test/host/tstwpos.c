/*
 * tstwpos.c - libc370 #200: the position of a write stream.
 *
 * ISSUE #200, measured on mvsdev JOB00495 (test/mvs/tstwrpos.c):
 *
 *   FTELL  ftell() after "L1\n", "L2\n", "AB" read 0 0 2; C wants 3 6 8.
 *          __fflush() set filepos = 0 on every record, so on a write
 *          stream it counted only the bytes since the last '\n'.
 *   SEEKW  "ABCDEF", fflush(), fseek(fp, 3, SEEK_SET), "xy" wrote two
 *          records, "ABCDEF" and "ABCxy".  After the flush filepos was 0,
 *          so the seek walked forward through __fgetc(), which on a write
 *          stream served the stale write buffer.
 *
 * The fix: a flush no longer resets filepos, and __fseek() on a stream not
 * open for reading never reads.  Until #189 brings w+/r+/a+, a write-only
 * stream can only "seek" to where it already is; anything else fails with
 * ESPIPE and leaves the stream and the data set as they were.
 *
 * This test compiles the REAL @@fflush.c, @@fputc.c, @@fgetc.c, @@fread.c
 * and @@fseek.c against shims for __awrite()/__aread() that record what
 * reaches the access method, and a __reopen() shim that only counts.
 *
 * CHECKS
 *   (1) ftell() after "L1\n", "L2\n", "AB" is 3, 6, 8 on V, and 81, 162,
 *       164 on F 80 - the byte view of #189                            RED
 *   (2) fseek(3) on a write stream at 6 fails, errno ESPIPE             RED
 *   (3) ...without reading and without reopening                        RED
 *   (4) ...and the next write lands as its own record: no "ABCxy"       RED
 *   (5) a failed seek does not set the error indicator
 *   (6) rewind-to-0 on a written stream fails, and does not reopen
 *       (a reopen for "w" would truncate the data set)                  RED
 *   (7) fseek(SEEK_CUR, 0) and fseek(SEEK_END, 0) on a write stream
 *       succeed: both name the current position                         RED
 *       ...and do not flush: a seek between two writes of one line must
 *       not split the line into two records (#189)
 *   (8) a read stream still counts: 3 fgetc() -> ftell() 3   regression
 *
 * BUILD / RUN (host, from test/host; same flag recipe as tsterrfl.c):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R/src/internal -o t tstwpos.c && ./t
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
/* string.h suppressed for the host, as in tsterrfl.c */
#define STRING_H
#include "src/internal/fileio.h"
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

/* ---- access-method shims ------------------------------------------------ */

#define MAXREC  8
static int              nrec;
static size_t           reclen[MAXREC];
static unsigned char    recdat[MAXREC][128];
static int              aread_calls;
static int              reopen_calls;
static unsigned char    aread_data[80];

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

int __aread(void *handle, void *buf, size_t *len)
{
    (void)handle;
    /* Bounded: before the fix, fseek(SEEK_END) on a write stream reads
       until EOF, and an endless shim turns that red case into a hang.  On
       MVS it is the S400 of #189 instead. */
    if (++aread_calls > 4) return -1;
    *(unsigned char **)buf = aread_data;
    *len = sizeof(aread_data);
    return 0;
}

FILE *__reopen(const char *fn, const char *mode, FILE *fp)
{
    (void)fn; (void)mode; (void)fp;
    reopen_calls++;
    return NULL;
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

#include "../../src/stdio/@@fflush.c"
#undef begwrite
#undef finwrite
#include "../../src/stdio/@@fputc.c"
#include <ctype.h>
#include "../../src/stdio/@@isbuf.c"   /* isspace() is a table macro */
#include "../../src/stdio/@@fgetc.c"
#include "../../src/stdio/@@fread.c"
#include "../../src/stdio/@@fseek.c"

/* ---- a FILE the shims can serve ---------------------------------------- */

static DCB              fake_dcb;
static unsigned char    filebuf[128];
static unsigned char    asmbuf[128];
static FILE             f;

static void mkfile(unsigned short flags)
{
    memset(&f, 0, sizeof(f));
    memcpy(f.eye, _FILE_EYE, sizeof(f.eye) - 1);
    memset(&fake_dcb, 0, sizeof(fake_dcb));
    fake_dcb.dcbdevt = 0x20;            /* anything but DCBDVTRM           */
    f.dcb     = &fake_dcb;
    f.asmbuf  = asmbuf;
    f.buf     = filebuf;
    f.upto    = filebuf;
    f.endbuf  = (flags & _FILE_FLAG_WRITE) ? filebuf + 80 : filebuf;
    f.lrecl   = 80;
    f.blksize = 800;
    f.recfm   = _FILE_RECFM_F;
    f.ungetch = -1;
    f.flags   = flags;
    nrec = aread_calls = reopen_calls = 0;
    memset(reclen, 0, sizeof(reclen));
    memset(recdat, 0, sizeof(recdat));
    errno = 0;
}

static void put(const char *s)
{
    while (*s) __fputc((unsigned char)*s++, &f);
}

static int anyrec(const char *s)
{
    int     i;
    size_t  n = strlen(s);

    for (i = 0; i < nrec && i < MAXREC; i++)
        if (memcmp(recdat[i], s, n) == 0) return 1;
    return 0;
}

#define TXW (_FILE_FLAG_OPEN | _FILE_FLAG_WRITE | _FILE_FLAG_DCBOUT)
#define TXR (_FILE_FLAG_OPEN | _FILE_FLAG_READ)

int main(void)
{
    long    t1, t2, t3;
    int     rc;
    int     e;

    printf("=== tstwpos: #200 - the position of a write stream ===\n\n");

    /* (1) ftell() counts from the start of the file - in the byte view a
       reader of the same data set sees (#189): on F 80 a text record is
       80 bytes plus '\n', on V it is what was written plus '\n' */
    mkfile(TXW);
    put("L1\n");    t1 = f.filepos;
    put("L2\n");    t2 = f.filepos;
    put("AB");      t3 = f.filepos;
    printf("  ftell F 80: %ld %ld %ld\n", t1, t2, t3);
    check(t1 == 81 && t2 == 162 && t3 == 164,
          "(1) F 80: ftell() after \"L1\\n\", \"L2\\n\", \"AB\" is 81, 162, 164");
    mkfile(TXW);
    f.recfm = _FILE_RECFM_V;
    f.lrecl = 84;
    put("L1\n");    t1 = f.filepos;
    put("L2\n");    t2 = f.filepos;
    put("AB");      t3 = f.filepos;
    printf("  ftell V 84: %ld %ld %ld\n", t1, t2, t3);
    check(t1 == 3 && t2 == 6 && t3 == 8,
          "(1v) V 84: ftell() after \"L1\\n\", \"L2\\n\", \"AB\" is 3, 6, 8");

    /* (2)-(5) seek backwards on a write stream */
    mkfile(TXW);
    put("ABCDEF");
    __fflush(&f);
    errno = 0;
    rc = __fseek(&f, 3, SEEK_SET);
    e = errno;
    printf("  fseek(3): rc=%d errno=%d aread=%d reopen=%d\n",
           rc, e, aread_calls, reopen_calls);
    check(rc != 0 && e == ESPIPE,
          "(2) fseek(3) on a write stream at 6 fails with ESPIPE");
    check(aread_calls == 0 && reopen_calls == 0 && f.upto == f.buf,
          "(3) ...without reading, reopening or moving the buffer");
    put("xy");
    __fflush(&f);
    check(nrec == 2 && anyrec("ABCDEF") && anyrec("xy") && !anyrec("ABCxy"),
          "(4) ...and \"xy\" is its own record: no \"ABCxy\"");
    check(!ferror(&f), "(5) a failed seek does not set the error indicator");

    /* (6) rewind-to-0 must not become a truncating reopen */
    mkfile(TXW);
    put("L1\n");
    errno = 0;
    rc = __fseek(&f, 0, SEEK_SET);
    e = errno;
    check(rc != 0 && e == ESPIPE && reopen_calls == 0,
          "(6) fseek(0) on a written stream fails and does not reopen");

    /* (7) seeks that name the current position */
    mkfile(TXW);
    put("L1\nAB");
    rc = __fseek(&f, 0, SEEK_CUR);
    /* no flush (#189): a flush would end "AB" as a record of its own */
    check(rc == 0 && f.filepos == 83 && aread_calls == 0 && nrec == 1,
          "(7a) fseek(0, SEEK_CUR) succeeds without flushing, stays at 83");
    rc = __fseek(&f, 0, SEEK_END);
    check(rc == 0 && f.filepos == 83 && aread_calls == 0 && reopen_calls == 0,
          "(7b) fseek(0, SEEK_END) on a write stream succeeds, stays at 83");
    rc = __fseek(&f, 83, SEEK_SET);
    put("CD\n");
    check(rc == 0 && nrec == 2 && anyrec("ABCD"),
          "(7c) fseek(ftell()) mid-line, then \"CD\\n\": one record ABCD");

    /* (8) a read stream still counts what it hands out */
    mkfile(TXR);
    memset(aread_data, 'R', sizeof(aread_data));
    __fgetc(&f); __fgetc(&f); __fgetc(&f);
    check(f.filepos == 3, "(8) read stream: 3 fgetc() -> filepos 3");

    printf("\n%s (%d failed)\n", bad ? "RED" : "GREEN", bad);
    return bad ? 1 : 0;
}
