/*
 * tstvarg.c - libc370 #382 on MVS: va_start() and va_arg() for every kind
 * of last named parameter.
 *
 * ISSUE #382: on MVS <stdarg.h> defined va_start(ap, last) as &last + 4.
 * That is right only for a last parameter that takes 4 bytes in the
 * parameter list.  A double or long long one takes 8, so every va_arg read
 * 4 bytes early; a char or short one is copied into the callee's frame, so
 * &last + 4 pointed at an uninitialised frame slot.  cc370's builtins
 * know the parameter list, and <stdarg.h> now uses them.
 *
 * double and long long are the C99 cases.  A char, short or float last
 * parameter is undefined behaviour (C99 7.15.1.4: its type changes under
 * the default promotions); the builtins handle it anyway and these
 * checks pin that, but a program should not rely on it.
 *
 * The defect is in the header, not in libc.a, so the red control is the
 * same source compiled against the old <stdarg.h>: TSTVARG with this
 * tree's include/, TSTVARGR with origin/main's stdarg.h copied over a
 * scratch include/ (main @ 7ca5ae8, before #382).
 *
 * Build:   cc370 -O1 -Wall -Werror -Iinclude \
 *                test/mvs/tstvarg.c -o TSTVARG -flinker-output=iebcopy
 *          mkdir -p old && cp -R include/ old/ && \
 *                git show 7ca5ae8:include/stdarg.h > old/stdarg.h
 *          cc370 -O1 -Wall -Werror -Iold \
 *                test/mvs/tstvarg.c -o TSTVARGR -flinker-output=iebcopy
 *          ld370 --pack TSTVARG=TSTVARG.iebcopy TSTVARGR=TSTVARGR.iebcopy \
 *                -o tstvarg -xmit --dsn IBMUSER.LIBC370.VARGSCR
 * Install: jcl/recvvarg.jcl.   Run: jcl/tstvarg.jcl.
 *
 * mvsdev JOB01362, 2026-10-05 (RECEIVE JOB01361): GREEN CC 0000, 8/8;
 * RED (stdarg.h before #382) CC 0001, 6 of 8 failed - all but the float
 * and int controls.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <stdarg.h>

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

/* Each function returns the first variadic argument, an int.  noinline
   keeps the call, and with it the parameter list, in place. */
#define NOINL __attribute__((noinline))

static NOINL int after_double(double d, ...)
{
    va_list ap; int r;
    va_start(ap, d); r = va_arg(ap, int); va_end(ap);
    return (r);
}

static NOINL int after_llong(long long x, ...)
{
    va_list ap; int r;
    va_start(ap, x); r = va_arg(ap, int); va_end(ap);
    return (r);
}

static NOINL int after_char(char c, ...)
{
    va_list ap; int r;
    va_start(ap, c); r = va_arg(ap, int); va_end(ap);
    return (r);
}

static NOINL int after_short(short s, ...)
{
    va_list ap; int r;
    va_start(ap, s); r = va_arg(ap, int); va_end(ap);
    return (r);
}

static NOINL int after_float(float f, ...)
{
    va_list ap; int r;
    va_start(ap, f); r = va_arg(ap, int); va_end(ap);
    return (r);
}

static NOINL int after_int(int i, ...)
{
    va_list ap; int r;
    va_start(ap, i); r = va_arg(ap, int); va_end(ap);
    return (r);
}

/* A mixed list after a double: int, double, long long, char * */
static NOINL int mixed(double d, ...)
{
    va_list ap; int i; double x; long long ll; const char *s;
    va_start(ap, d);
    i  = va_arg(ap, int);
    x  = va_arg(ap, double);
    ll = va_arg(ap, long long);
    s  = va_arg(ap, const char *);
    va_end(ap);
    return (i == 7 && x == 2.5 && ll == 0x123456789LL
            && s[0] == 'o' && s[1] == 'k');
}

/* va_copy after a char: both lists read the same arguments */
static NOINL int copied(char c, ...)
{
    va_list ap, cp; int a, b;
    va_start(ap, c);
    va_copy(cp, ap);
    a = va_arg(ap, int);
    b = va_arg(cp, int);
    va_end(cp);
    va_end(ap);
    return (a == 99 && b == 99);
}

/* volatile, so the arguments are loaded at run time */
static volatile double      vd = 1.5;
static volatile long long   vll = 0x0102030405060708LL;
static volatile char        vc = 'c';
static volatile short       vs = -2;
static volatile float       vf = 0.25f;
static volatile int         vi = 3;

int main(void)
{
    printf("=== tstvarg: va_start/va_arg by last parameter (#382) ===\n");

    CHECK(after_double(vd, 42) == 42,  "last parameter double");
    CHECK(after_llong(vll, 42) == 42,  "last parameter long long");
    CHECK(after_char(vc, 42) == 42,    "last parameter char");
    CHECK(after_short(vs, 42) == 42,   "last parameter short");
    CHECK(after_float(vf, 42) == 42,   "last parameter float (control)");
    CHECK(after_int(vi, 42) == 42,     "last parameter int (control)");
    CHECK(mixed(vd, 7, 2.5, 0x123456789LL, "ok"),
          "double, then int, double, long long, char *");
    CHECK(copied(vc, 99),              "va_copy after a char");

    printf("=== tstvarg: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0)
        printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return (mbt_failed > 0 ? 1 : 0);
}
