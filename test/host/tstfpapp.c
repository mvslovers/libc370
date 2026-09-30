/*
 * tstfpapp.c - libc370 #198: "a" asks __aopen() for EXTEND.
 *
 * ISSUE #198: fopen(..., "a") overwrote the data set.  __fpmode() set
 * _FILE_FLAG_APPEND and nothing read it; __fpopen() passed __aopen() mode 0
 * or 1 only, so "a" was OPEN OUTPUT.  Measured on mvsdev JOB00490: by name,
 * as a member and through a DISP=OLD DD the old record was gone, and only a
 * DISP=MOD DD appended.  @@aopen.asm has taken mode 3 = EXTEND all along.
 *
 * This test compiles the REAL @@fpmode.c and @@fpopen.c against an
 * __aopen() shim that records the mode it is handed.
 *
 * CHECKS
 *   (1) "r"        -> mode 0                              regression
 *   (2) "w"        -> mode 1                              regression
 *   (3) "a"        -> mode 3 (EXTEND)                     RED
 *   (4) "ab"       -> mode 3                              RED
 *   (5) "a,bsam"   -> mode 11 (EXTEND + BSAM)             RED
 *   (6) "w,bsam"   -> mode 9                              regression
 *
 * The member half of #198 (an existing member is refused, a new one is
 * created) lives in fopen.c and needs a real PDS; test/mvs/tstappnd.c
 * measures it.
 *
 * BUILD / RUN (host, from test/host; same flag recipe as tsterrfl.c):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R/src/internal -o t tstfpapp.c && ./t
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
char   *strstr(const char *h, const char *n);

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ibm/mvs/dcbd.h>
#include <ibm/mvs/iefjfcbn.h>

static int  errno_cell;
int *__errno(void) { return &errno_cell; }

static int  bad = 0;

static void check(int cond, const char *what)
{
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* ---- shims -------------------------------------------------------------- */

static int  seen_mode;
static DCB  fake_dcb;

void *__aopen(const char *ddname, int *mode, int *recfm, int *lrecl,
              int *blksize, void **asmbuf, const char *member)
{
    (void)ddname; (void)recfm; (void)lrecl; (void)blksize; (void)member;
    static unsigned char ab[128];
    seen_mode = *mode;
    *asmbuf = ab;
    return &fake_dcb;
}

int __ddbusy(FILE *fp)                  { (void)fp; return 0; }
int __aclose(void *handle)              { (void)handle; return 0; }
int __rdjfcb(DCB *dcb, JFCB *jfcb)      { (void)dcb; (void)jfcb; return 0; }

/* libc370's tolower() is a table macro over __tolow.  The library's table
   (@@tolow.c) is EBCDIC and would fold ASCII letters wrongly here, so the
   host's own tolower() fills it. */
#include <ctype.h>
static short    tolow_tbl[256];
short           *__tolow = tolow_tbl;
#undef tolower
extern int tolower(int);
static void init_tolow(void)
{
    int c;
    for (c = 0; c < 256; c++) tolow_tbl[c] = (short)tolower(c);
}
#define tolower(c) (__tolow[(c)])

/* ---- the real thing ---------------------------------------------------- */

#include "../../src/clib/@@fpmode.c"
#include "../../src/clib/@@fpopen.c"

static int modeof(const char *m)
{
    static FILE f;

    memset(&f, 0, sizeof(f));
    strcpy(f.ddname, "TESTDD");
    memset(&fake_dcb, 0, sizeof(fake_dcb));
    fake_dcb.dcbrecfm = 0x80;           /* F                              */
    fake_dcb.dcblrecl = 80;
    fake_dcb.dcbblksi = 800;
    seen_mode = -1;
    if (__fpmode(&f, m)) return -2;
    if (__fpopen(&f))    return -3;
    free(f.buf);
    return seen_mode;
}

int main(void)
{
    int m;

    init_tolow();
    printf("=== tstfpapp: #198 - \"a\" asks for EXTEND ===\n\n");

    m = modeof("r");       printf("  \"r\"      -> %d\n", m);
    check(m == 0,  "(1) \"r\" -> mode 0 (INPUT)");
    m = modeof("w");       printf("  \"w\"      -> %d\n", m);
    check(m == 1,  "(2) \"w\" -> mode 1 (OUTPUT)");
    m = modeof("a");       printf("  \"a\"      -> %d\n", m);
    check(m == 3,  "(3) \"a\" -> mode 3 (EXTEND)");
    m = modeof("ab");      printf("  \"ab\"     -> %d\n", m);
    check(m == 3,  "(4) \"ab\" -> mode 3 (EXTEND)");
    m = modeof("a,bsam");  printf("  \"a,bsam\" -> %d\n", m);
    check(m == 11, "(5) \"a,bsam\" -> mode 11 (EXTEND + BSAM)");
    m = modeof("w,bsam");  printf("  \"w,bsam\" -> %d\n", m);
    check(m == 9,  "(6) \"w,bsam\" -> mode 9 (OUTPUT + BSAM)");

    printf("\n%s (%d failed)\n", bad ? "RED" : "GREEN", bad);
    return bad ? 1 : 0;
}
