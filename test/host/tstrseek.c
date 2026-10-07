/*
 * tstrseek.c - libc370 #473: fseek() inside the buffer of an "r" stream.
 *
 * ISSUE #473, measured from brexx370 on mvsdev with 2.6.0: after one
 * 80-byte FB record of a text stream was read, fseek(f, 0, SEEK_SET)
 * returned 0 and ftell() still said 81.  __fseek() took its in-buffer
 * shortcut with oldpos 81 and start 0 and set upto = buf + (newpos -
 * oldpos) = buf - 81, without touching filepos.  The next fgets() read the
 * 81 bytes in front of the buffer, so the result depended on the load
 * module's layout: brexx370's LINEIN(file, n) passed, and failed with 2 KB
 * more unreferenced data in the module.
 *
 * The fix: the offset into the buffer is taken from the buffer's start,
 * upto = buf + (newpos - start), and filepos = newpos.
 *
 * This test compiles the REAL @@fgetc.c, @@fgets.c and @@fseek.c (and what
 * they call) against an __aread() shim serving three FB 80 records whose
 * bytes name their position, and a __reopen() shim that rewinds the shim.
 * The buffer sits behind 128 bytes of '#', so a read in front of it is
 * seen as '#' instead of depending on the host's data layout.
 *
 * CHECKS
 *   (1) one record read with fgets(), then fseek(0, SEEK_SET): ftell() is
 *       0, the next fgetc() is the first byte, no reopen               RED
 *   (2) ...and the next two fgets() return lines 1 and 2 (brexx370's
 *       LINEIN(file, 3) after LINEIN(file, 1))                        RED
 *   (3) fgetc() twice, fseek(ftell(), SEEK_SET): ftell() 2, the next
 *       fgetc() is the third byte                                     RED
 *   (4) fgetc() three times, fseek(-1, SEEK_CUR): ftell() 2, the next
 *       fgetc() is the third byte again                               RED
 *   (5) fgetc() once, fseek(10, SEEK_SET) forward inside the buffer:
 *       the next fgetc() is byte 10                                   RED
 *   (6) a target in front of the buffer still reopens: after line 1
 *       and one byte of line 2, fseek(0) reopens once and the next
 *       fgetc() is the first byte                               regression
 *
 * BUILD / RUN (host, from test/host; same flag recipe as tstwpos.c):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R -o t tstrseek.c && ./t
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

/* Three FB 80 records.  Byte 0 is the record's letter, byte i the digit
   i % 10, so a byte read back names where it came from. */
#define NREC    3
static unsigned char    recs[NREC][80];
static int              aread_calls;
static int              reopen_calls;

int __awrite(void *handle, unsigned char **buf, size_t *sz)
{
    (void)handle; (void)buf; (void)sz;
    printf("  !! __awrite() reached from a read stream\n");
    return -1;
}

int __aread(void *handle, void *buf, size_t *len)
{
    (void)handle;
    if (aread_calls >= NREC) return -1;
    *(unsigned char **)buf = recs[aread_calls++];
    *len = 80;
    return 0;
}

/* a reopen for "r" starts the data set again */
FILE *__reopen(const char *fn, const char *mode, FILE *fp)
{
    (void)fn; (void)mode;
    reopen_calls++;
    aread_calls = 0;
    fp->upto    = fp->buf;
    fp->endbuf  = fp->buf;
    fp->filepos = 0;
    fp->ungetch = -1;
    return fp;
}

/* a plain "r" stream never turns round (#189); a call is a failure */
int __fpswt(FILE *fp, int out)
{
    (void)fp; (void)out;
    printf("  !! __fpswt() reached from a plain stream\n");
    errno = EBADF;
    return -1;
}
int __fpupc(FILE *fp, int c)
{
    (void)fp; (void)c;
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
#include "../../src/stdio/@@fgets.c"
#include "../../src/stdio/@@fread.c"
#include "../../src/stdio/@@fseek.c"

/* ---- a FILE the shims can serve ---------------------------------------- */

/* the buffer behind 128 bytes of '#': a read in front of it shows */
static DCB              fake_dcb;
static unsigned char    arena[256];
static unsigned char    asmbuf[128];
static FILE             f;

static void mkfile(void)
{
    int     r, i;

    for (r = 0; r < NREC; r++) {
        recs[r][0] = (unsigned char)('A' + r);
        for (i = 1; i < 80; i++) recs[r][i] = (unsigned char)('0' + i % 10);
    }
    memset(arena, '#', sizeof(arena));
    memset(&f, 0, sizeof(f));
    memcpy(f.eye, _FILE_EYE, sizeof(f.eye) - 1);
    memset(&fake_dcb, 0, sizeof(fake_dcb));
    fake_dcb.dcbdevt = 0x20;            /* anything but DCBDVTRM           */
    f.dcb     = &fake_dcb;
    f.asmbuf  = asmbuf;
    f.buf     = arena + 128;
    f.upto    = f.buf;
    f.endbuf  = f.buf;
    f.lrecl   = 80;
    f.blksize = 800;
    f.recfm   = _FILE_RECFM_F;
    f.ungetch = -1;
    f.flags   = _FILE_FLAG_OPEN | _FILE_FLAG_READ;
    aread_calls = reopen_calls = 0;
    errno = 0;
}

int main(void)
{
    char    line[100];
    int     rc, c, c2;
    long    pos;
    long    pos0;
    char    *l1, *l2;

    printf("=== tstrseek: #473 - fseek() inside the buffer of \"r\" ===\n\n");

    /* (1)-(2) brexx370's LINEIN: line 1, then rewind and skip to line 2 */
    mkfile();
    __fgets(line, sizeof(line), &f);
    pos = f.filepos;
    rc  = __fseek(&f, 0, SEEK_SET);
    pos0 = f.filepos;
    printf("  after line 1: ftell %ld; fseek(0) rc=%d ftell %ld reopen=%d\n",
           pos, rc, pos0, reopen_calls);
    c = __fgetc(&f);
    printf("  next fgetc: '%c'\n", c);
    check(pos == 81 && rc == 0 && pos0 == 0 && c == 'A'
          && reopen_calls == 0,
          "(1) fseek(0) after one record: back at 0, first byte 'A'");
    mkfile();
    __fgets(line, sizeof(line), &f);
    __fseek(&f, 0, SEEK_SET);
    l1 = __fgets(line, sizeof(line), &f);
    c  = l1 ? line[0] : 0;
    l2 = __fgets(line, sizeof(line), &f);
    c2 = l2 ? line[0] : 0;
    printf("  lines after the rewind start with '%c', '%c'; ftell %ld\n",
           c ? c : '?', c2 ? c2 : '?', (long)f.filepos);
    check(c == 'A' && c2 == 'B' && f.filepos == 162,
          "(2) ...the next two fgets() are lines 1 and 2, ftell 162");

    /* (3) fseek(ftell()) in the middle of a record stays where it is */
    mkfile();
    __fgetc(&f); __fgetc(&f);
    pos = f.filepos;
    rc  = __fseek(&f, pos, SEEK_SET);
    pos = f.filepos;
    c   = __fgetc(&f);
    printf("  fseek(ftell()=2): rc=%d ftell %ld, next '%c'\n", rc, pos, c);
    check(rc == 0 && pos == 2 && c == '2',
          "(3) fseek(ftell(), SEEK_SET) mid-record: ftell 2, next '2'");

    /* (4) one byte back */
    mkfile();
    __fgetc(&f); __fgetc(&f); __fgetc(&f);
    rc  = __fseek(&f, -1, SEEK_CUR);
    pos = f.filepos;
    c   = __fgetc(&f);
    printf("  fseek(-1, SEEK_CUR) at 3: rc=%d ftell %ld, next '%c'\n",
           rc, pos, c);
    check(rc == 0 && pos == 2 && c == '2',
          "(4) fseek(-1, SEEK_CUR): ftell 2, the previous byte '2' again");

    /* (5) forward inside the buffer */
    mkfile();
    __fgetc(&f);
    rc  = __fseek(&f, 10, SEEK_SET);
    pos = f.filepos;
    c   = __fgetc(&f);
    printf("  fseek(10) at 1: rc=%d ftell %ld, next '%c'\n", rc, pos, c);
    check(rc == 0 && pos == 10 && c == '0' && aread_calls == 1,
          "(5) fseek(10) forward in the buffer: ftell 10, next byte 10 '0'");

    /* (6) a target in front of the buffer reopens */
    mkfile();
    __fgets(line, sizeof(line), &f);
    __fgetc(&f);                        /* 'B': the buffer is record 2 */
    rc  = __fseek(&f, 0, SEEK_SET);
    pos = f.filepos;
    c   = __fgetc(&f);
    printf("  fseek(0) at 82: rc=%d reopen=%d ftell %ld, next '%c'\n",
           rc, reopen_calls, pos, c);
    check(rc == 0 && reopen_calls == 1 && pos == 0 && c == 'A',
          "(6) fseek(0) in front of the buffer reopens, next 'A'");

    printf("\n%s (%d failed)\n", bad ? "RED" : "GREEN", bad);
    return bad ? 1 : 0;
}
