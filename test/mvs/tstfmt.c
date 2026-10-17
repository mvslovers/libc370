/*
 * tstfmt.c - libc370 size analysis F2: the formatter takes its output
 *            functions as pointers.  The text must not change.
 *
 * One program, built against this tree (FMTN) and the installed library
 * (FMTR).  It formats the same set of conversions four ways - printf() to
 * SYSPRINT, fprintf() to DD OUT, sprintf() and snprintf() into a buffer
 * written to SYSPRINT - and checks what it can itself: the sprintf() and
 * vfprintf() results agree, snprintf() truncates and terminates, %n counts,
 * and fprintf() on a stream open for reading fails with EBADF.  The JCL
 * steps' SYSPRINT and OUT must be identical between FMTN and FMTR.
 *
 * RC 0 = every check passed, 8 = one did not.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <mvs/wto.h>

static int bad;
static char *volatile nullstr;     /* %s of NULL prints "(null)" */

static void want(int cond, const char *what)
{
    wtof("TSTFMT %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

#define FMT "[%d|%5d|%-5d|%05d|%+d|%u|%x|%X|%#x|%o|%c|%%|%s|%.3s|%8s|%-8s|" \
            "%ld|%lld|%llx|%f|%.2f|%e|%g|%p]"
#define ARGS -42, 7, 7, 7, 7, 4000000000u, 0xbeef, 0xbeef, 0xbeef, 8, 'Q', \
             "text", "abcdef", "r", "l", -123456L, 1234567890123LL, \
             0x123456789abcLL, 3.25, 2.0 / 3.0, 12345.678, 0.0001, (void *)0x1234

int main(void)
{
    char    buf[400];
    char    small[8];
    FILE    *out = fopen("dd:OUT", "w");
    FILE    *in  = fopen("dd:IN", "r");
    int     n1, n2, n;

    n1 = printf(FMT "\n", ARGS);
    n2 = sprintf(buf, FMT "\n", ARGS);
    want(n1 == n2 && n1 == (int)strlen(buf), "printf() and sprintf() count alike");
    fputs(buf, stdout);

    want(out != NULL, "fopen(dd:OUT)");
    if (out) {
        want(fprintf(out, FMT "\n", ARGS) == n2, "fprintf() counts as sprintf()");
        fprintf(out, "null %s, char %c, done\n", nullstr, 'z');
        fclose(out);
    }

    n = snprintf(small, sizeof(small), "%s-%d", "abcdefgh", 12);
    want(n == 11 && strcmp(small, "abcdefg") == 0, "snprintf() truncates and terminates");
    printf("snprintf=%d [%s]\n", n, small);

    n = 0;
    printf("abc%n|%s\n", &n, "after");
    want(n == 3, "%n counts what printf() wrote");
    sprintf(buf, "xy%nz", &n);
    want(n == 2, "%n counts what sprintf() wrote");

    want(in != NULL, "fopen(dd:IN, r)");
    if (in) {
        errno = 0;
        n = fprintf(in, "x%d", 1);
        want(n < 0 && errno == EBADF, "fprintf() on a read stream: -1, EBADF");
        fclose(in);
    }

    wtof("TSTFMT VERDICT failed=%d", bad);
    return bad ? 8 : 0;
}
