/*
 * tstplus.c - libc370 #189 slice 1: r+, w+ and a+.
 *
 * Until #189 __fpmode() refused every '+'.  Slice 1: a '+' stream may read
 * and write; its DCB is opened one way at a time and __fpswt() turns it
 * round, reopening the same DD.  Reads go anywhere, writes only at the end
 * (EXTEND) - overwriting in the middle is slice 2 and answers EOPNOTSUPP.
 *
 * This test compiles the REAL @@fpmode, @@fpopen, @@fflush, @@fputc,
 * @@fgetc, @@fread, @@fseek, @@fpswt and fseek/ftell against an in-memory
 * data set behind __aopen()/__aread()/__awrite()/__aclose(): FB 80 on
 * DASD (DCBDEVT 3380), records kept as the access method would keep them.
 * OPEN INPUT reads from record 1, OUTPUT empties the data set, EXTEND
 * appends - which is what makes "a direction switch must not truncate"
 * testable at all.
 *
 * CHECKS
 *   mode parsing: r+ w+ a+ rb+ r+b accepted, "+", "x+" and "r+,record"
 *   refused; the first DCB direction per mode.
 *   w+   write L1 L2, fseek(0), read back L1 L2 then EOF; the data set was
 *        written once with OUTPUT and never truncated again
 *   w+   after reading to the end a write appends (EXTEND): L1 L2 L3
 *   w+   a write in the middle fails EOPNOTSUPP and changes nothing
 *   r+   opens without truncating; reads; SEEK_END + write appends
 *   a+   ftell() after open is the size; a write appends; fseek(0) reads
 *        everything; a write after reading from the start still appends
 *   a+   a read without a seek starts at the end: EOF
 *   seek to the current position, and forward, do not reopen (brexx370)
 *   '+' on a non-DASD device is refused with EINVAL
 *   the byte view: ftell() after "L1\n" on FB 80 is 81, matching a reader
 *
 * BUILD / RUN (host, from test/host):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -o t tstplus.c && ./t
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
/* clibstr.h suppressed for the host, as in tsterrfl.c */
#define CLIBSTR_H
#include <stddef.h>
void   *memset(void *s, int c, size_t n);
void   *memcpy(void *t, const void *s, size_t n);
size_t  strlen(const char *s);
int     strcmp(const char *a, const char *b);
char   *strcpy(char *t, const char *s);
char   *strstr(const char *h, const char *n);
void   *memchr(const void *s, int c, size_t n);
int     memcmp(const void *a, const void *b, size_t n);
char   *strdup(const char *s);

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <osdcb.h>
#include <osjfcb.h>

static int  errno_cell;
int *__errno(void) { return &errno_cell; }

static int  bad = 0;

static void check(int cond, const char *what)
{
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* ---- the in-memory data set --------------------------------------------- */

#define MAXREC  16
static unsigned char    disk[MAXREC][80];
static int              nrec;           /* records on "disk"               */
static int              cur;            /* next record to read             */
static int              n_open, n_output, n_extend, n_input, n_close;
static unsigned char    devt = 0x2E;    /* 3380                            */
static DCB              fake_dcb;
static unsigned char    ab[128];

void *__aopen(const char *ddname, int *mode, int *recfm, int *lrecl,
              int *blksize, void **asmbuf, const char *member)
{
    (void)ddname; (void)recfm; (void)lrecl; (void)blksize; (void)member;
    n_open++;
    switch (*mode & 7) {
    case 0:  n_input++;  cur = 0;               break;
    case 1:  n_output++; nrec = 0; cur = 0;     break;
    case 3:  n_extend++; cur = nrec;            break;
    default: return (void *)-37;
    }
    memset(&fake_dcb, 0, sizeof(fake_dcb));
    fake_dcb.dcbdevt  = devt;
    fake_dcb.dcbrecfm = 0x80;           /* F                              */
    fake_dcb.dcblrecl = 80;
    fake_dcb.dcbblksi = 800;
    *asmbuf = ab;
    return &fake_dcb;
}

int __aread(void *handle, void *buf, size_t *len)
{
    (void)handle;
    if (cur >= nrec) return -1;
    *(unsigned char **)buf = disk[cur++];
    *len = 80;
    return 0;
}

int __awrite(void *handle, unsigned char **buf, size_t *sz)
{
    (void)handle;
    if (nrec < MAXREC) memcpy(disk[nrec++], *buf, 80);
    cur = nrec;
    return 0;
}

void __aclose(void *handle)             { (void)handle; n_close++; }
int  __ddbusy(FILE *fp)                 { (void)fp; return 0; }
int  __rdjfcb(DCB *dcb, JFCB *jfcb)     { (void)dcb; (void)jfcb; return 0; }
FILE *__reopen(const char *fn, const char *mode, FILE *fp)
{
    (void)fn; (void)mode; (void)fp;
    printf("  !! __reopen() reached - a '+' stream must never get here\n");
    bad++;
    return NULL;
}
int lock(void *thing, int shr)          { (void)thing; (void)shr; return 0; }
int unlock(void *thing, int shr)        { (void)thing; (void)shr; return 0; }

/* libc370's tolower() is a table over __tolow; the library's table is
   EBCDIC, so the host's own fills it (as in tstfpapp.c) */
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

#include "../../src/clib/@@isbuf.c"   /* isspace() is a table macro */
#include "../../src/clib/@@fpmode.c"
#include "../../src/clib/@@fpopen.c"
#include "../../src/clib/@@fflush.c"
#undef begwrite
#undef finwrite
#include "../../src/clib/@@fputc.c"
#include "../../src/clib/@@fgetc.c"
#include "../../src/clib/@@fread.c"
#include "../../src/clib/@@fseek.c"
#include "../../src/clib/@@fpswt.c"
#undef fseek
#undef ftell
#include "../../src/clib/fseek.c"
#include "../../src/clib/ftell.c"

/* ---- helpers ------------------------------------------------------------ */

static FILE f;

static void preload(const char *const *lines, int n)
{
    int i;
    nrec = 0;
    for (i = 0; i < n; i++) {
        memset(disk[i], ' ', 80);
        memcpy(disk[i], lines[i], strlen(lines[i]));
    }
    nrec = n;
}

static int op(const char *mode)
{
    memset(&f, 0, sizeof(f));
    strcpy(f.ddname, "TESTDD");
    n_open = n_output = n_extend = n_input = n_close = 0;
    errno = 0;
    if (__fpmode(&f, mode)) return -2;
    if (__fpopen(&f))       return -3;
    return 0;
}

static void put(const char *s) { while (*s) __fputc((unsigned char)*s++, &f); }

/* one line, trailing blanks and the '\n' cut off; NULL at EOF */
static char *line(void)
{
    static char b[100];
    int         c, n = 0;

    while ((c = __fgetc(&f)) != EOF && c != '\n' && n < 99) b[n++] = (char)c;
    if (c == EOF && n == 0) return NULL;
    while (n > 0 && b[n-1] == ' ') n--;
    b[n] = 0;
    return b;
}

static int ondisk(const char *const *want, int n)
{
    int i;
    char r[81];
    if (nrec != n) return 0;
    for (i = 0; i < n; i++) {
        size_t k = 80;
        memcpy(r, disk[i], 80);
        while (k > 0 && r[k-1] == ' ') k--;
        r[k] = 0;
        if (strcmp(r, want[i]) != 0) return 0;
    }
    return 1;
}

static void shut(void)
{
    __fflush(&f);
    __aclose(f.dcb);
    free(f.buf);
}

int main(void)
{
    static const char *const L12[]  = { "L1", "L2" };
    static const char *const L123[] = { "L1", "L2", "L3" };
    static const char *const L1234[]= { "L1", "L2", "L3", "L4" };
    char    *s1, *s2, *s3;
    long    t;
    int     rc, e, before;

    init_tolow();
    printf("=== tstplus: #189 slice 1 - r+, w+, a+ ===\n\n");

    printf("mode parsing\n");
    check(op("r+") == 0 && (f.flags & _FILE_FLAG_READ) && (f.flags & _FILE_FLAG_WRITE)
          && !(f.flags & _FILE_FLAG_DCBOUT) && n_input == 1,
          "r+ : read+write, DCB opens INPUT");
    check(op("w+") == 0 && (f.flags & _FILE_FLAG_DCBOUT) && n_output == 1,
          "w+ : DCB opens OUTPUT");
    check(op("a+") == 0 && (f.flags & _FILE_FLAG_DCBOUT) && n_extend == 1
          && (f.flags & _FILE_FLAG_POSEND),
          "a+ : DCB opens EXTEND, position = end (not yet counted)");
    check(op("rb+") == 0 && op("r+b") == 0, "rb+ and r+b accepted");
    check(op("+") == -2 && op("x+") == -2 && op("r+,record") == -2,
          "\"+\", \"x+\" and \"r+,record\" refused");
    check(op("w") == 0 && (f.flags & _FILE_FLAG_DCBOUT) && !(f.flags & _FILE_FLAG_READ),
          "plain w: writes, cannot read (unchanged)");

    printf("w+\n");
    nrec = 0;
    op("w+");
    put("L1\nL2\n");
    t = ftell(&f);
    printf("  ftell after \"L1\\nL2\\n\" on FB 80: %ld\n", t);
    check(t == 162, "the byte view: ftell() = 2 x 81");
    rc = __fseek(&f, 0, SEEK_SET);
    s1 = line(); s1 = s1 ? strdup(s1) : NULL;
    s2 = line(); s2 = s2 ? strdup(s2) : NULL;
    s3 = line();
    check(rc == 0 && s1 && s2 && !strcmp(s1, "L1") && !strcmp(s2, "L2") && !s3,
          "write L1 L2, fseek(0), read L1 L2 EOF");
    check(n_output == 1 && ondisk(L12, 2), "written once with OUTPUT, not truncated since");
    put("L3\n");
    shut();
    check(n_extend == 1 && ondisk(L123, 3), "after reading to the end, a write appends (EXTEND)");
    free(s1); free(s2);

    op("w+");
    put("L1\nL2\n");
    __fseek(&f, 0, SEEK_SET);
    line();
    errno = 0;
    rc = __fputc('X', &f);
    e = errno;
    shut();
    check(rc == EOF && e == EOPNOTSUPP && ondisk(L12, 2),
          "a write in the middle fails EOPNOTSUPP and changes nothing");

    printf("r+\n");
    preload(L123, 3);
    op("r+");
    check(n_output == 0 && ondisk(L123, 3), "opens without truncating");
    s1 = line();
    check(s1 && !strcmp(s1, "L1"), "reads L1");
    rc = __fseek(&f, 0, SEEK_END);
    t = ftell(&f);
    put("L4\n");
    shut();
    printf("  r+ SEEK_END: rc=%d ftell=%ld nrec=%d extend=%d\n", rc, t, nrec, n_extend);
    check(rc == 0 && t == 243 && n_output == 0 && n_extend == 1 && ondisk(L1234, 4),
          "SEEK_END (243) + write appends L4 - EXTEND, never OUTPUT");

    printf("a+\n");
    preload(L12, 2);
    op("a+");
    t = ftell(&f);
    check(t == 162, "ftell() after open = size (162)");
    put("L3\n");
    t = ftell(&f);
    check(t == 243, "ftell() after appending L3 = 243");
    rc = __fseek(&f, 0, SEEK_SET);
    s1 = line(); s1 = s1 ? strdup(s1) : NULL;
    check(rc == 0 && s1 && !strcmp(s1, "L1"), "fseek(0) reads L1");
    put("L4\n");                        /* a+ writes at the end, always  */
    shut();
    check(ondisk(L1234, 4), "a write after reading from the start still appends");
    free(s1);

    preload(L12, 2);
    op("a+");
    s1 = line();
    shut();
    check(s1 == NULL && ondisk(L12, 2), "a read without a seek starts at the end: EOF");

    printf("a line built from several writes (brexx370 CHAROUT)\n");
    nrec = 0;
    op("w");
    put("AB");
    t = ftell(&f);
    rc = __fseek(&f, t, SEEK_SET);
    put("CD\n");
    shut();
    {
        static const char *const ABCD[] = { "ABCD" };
        check(t == 2 && rc == 0 && ondisk(ABCD, 1), "plain w: \"AB\", fseek(ftell()), \"CD\\n\" -> one record ABCD");
        nrec = 0;
        op("w+");
        put("AB");
        t = ftell(&f);
        before = n_open;
        rc = __fseek(&f, t, SEEK_SET);
        put("CD\n");
        shut();
        check(t == 2 && rc == 0 && n_open == before && ondisk(ABCD, 1),
              "w+: the same, no reopen -> one record ABCD");
    }
    {
        static const char *const L12ABCD[] = { "L1", "L2", "ABCD" };
        preload(L12, 2);
        op("a+");
        /* what fopen() does for "a+": count the size now, then EXTEND */
        rc = __fpswt(&f, 0) | __fpswt(&f, 1);
        before = n_open;
        put("AB");
        t = ftell(&f);
        __fseek(&f, t, SEEK_SET);
        put("CD\n");
        shut();
        check(rc == 0 && t == 164 && n_open == before && ondisk(L12ABCD, 3),
              "a+: counted at open (162), \"AB\" -> ftell 164 without a flush, one record ABCD");
    }

    printf("cheap seeks (brexx370)\n");
    preload(L123, 3);
    op("r+");
    line();                             /* at 81 */
    before = n_open;
    rc = __fseek(&f, 81, SEEK_SET);
    check(rc == 0 && n_open == before, "fseek to the current position: no reopen");
    rc = __fseek(&f, 162, SEEK_SET);
    s1 = line();
    check(rc == 0 && n_open == before && s1 && !strcmp(s1, "L3"),
          "fseek forward: no reopen, reads L3");
    rc = __fseek(&f, 0, SEEK_SET);
    s1 = line();
    check(rc == 0 && n_open == before + 1 && s1 && !strcmp(s1, "L1"),
          "fseek backward past the record: one reopen, reads L1");
    shut();

    printf("devices\n");
    devt = DCBDVTRM;
    rc = op("w+");
    e = errno;
    devt = 0x2E;
    check(rc == -3 && e == EINVAL, "'+' on a terminal: EINVAL");

    printf("\n%s (%d failed)\n", bad ? "RED" : "GREEN", bad);
    return bad ? 1 : 0;
}
