/*
 * tstkey0.c - libc370 #693 on MVS: what the library creates on first use
 * inside a key-0 window must still be usable after it, in the program's
 * own key.  (APF-AUTHORIZED, run from SYS2.LINKLIB, batch, key 8.)
 *
 * In supervisor state with PSW key 0, GETMAIN takes subpool 0 from subpool
 * 252, key-0 storage (IEAVGM00, PGTSPQE: only for key 0 AND supervisor
 * state).  So whatever malloc() returns inside __super(PSWKEY0, ...) is
 * key 0, and a store into it after __prob() is a protection exception.
 * Each case does a first use inside such a window, returns to key 8 and
 * uses the same structure again; the key-8 half runs under try().
 *
 *   A  fopen()/fclose() of a STEPLIB member inside the window (what
 *      __loadhi() does), then fopen() of the same member in key 8.  Up to
 *      2.24.0 the first fopen() created grt->grtfile in key 0 and the
 *      second one ended S0C4 in arrayadd().
 *   D  (PARM='D', a step of its own: it needs a fresh list) 20 files
 *      open in key 8 fill the list; the 21st fopen(), inside the window,
 *      grows it; then fopen() in key 8.  Up to 2.24.0 the grown list was
 *      key 0 (and the free() of the old one, in key 0, ended S30A).
 *   B  the first getenv() inside the window loads DD SYSENV; then, in
 *      key 8, (B1) setenv() of a new name, (B2) setenv() of a name SYSENV
 *      set, (B3) both read back with getenv().
 *   C  the first fputs() to stdout inside the window opens it; then
 *      fputs() and fflush() in key 8.
 *
 * Each case says PASS or FAIL with the abend code through WTO; an abend
 * inside a window is caught and counts as a failure.  The job
 * needs //SYSENV DD * with K0VAR=ONE and a SYSPRINT DD.
 *
 * BUILD (host) - AC=1 on the ld370 link, and again on the pack:
 *     cc370 -O1 -Wall -Werror -Iinclude -c test/mvs/tstkey0.c -o k0.o
 *     ld370 --entry @@CRT0 --ac 1 -iebcopy -o TSTKEY0G k0.o \
 *           -Lbuild/sdk -lc
 *     (TSTKEY0R: the same object against a 2.24.0 build/sdk)
 *     ld370 --pack --ac 1 TSTKEY0G=TSTKEY0G.iebcopy \
 *           TSTKEY0R=TSTKEY0R.iebcopy -o k0 -xmit \
 *           --dsn IBMUSER.LIBC370.K0SCR
 * Upload:  k0.xmit to IBMUSER.LIBC370.K0XMIT (FB 80, binary)
 * Install: jcl/recvkey0.jcl.   Run: jcl/tstkey0.jcl (own container only:
 * it copies both modules into SYS2.LINKLIB and removes them again).
 *
 * Own MVS/CE container, 2026-10-20: TSTKEY0G all PASS, RC 0 in both
 * steps; TSTKEY0R (2.24.0) FAIL A S0C4, B1 S0C4, B2 S30A, B3, C S0C4,
 * the step ending S0C4 at its close of the key-0 files, and D S30A
 * inside the window, RC 8.
 *
 * RC 0 = every case passed, 8 = one did not, 12 = not APF authorized.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mvs/wto.h>
#include <mvs/apf.h>
#include <mvs/recovery.h>

#define MEMBER  "DD:STEPLIB(TSTKEY0G)"

static int bad;

static void want(int cond, const char *what, unsigned abend)
{
    wtof("TSTKEY0 %s: %s (abend %06X)", cond ? "PASS" : "FAIL", what,
         abend);
    if (!cond) bad++;
}

/* run fn(arg) in supervisor state, key 0, and come back to the key we
   came in with; 0 = done, else __super()'s rc.  An abend inside is
   caught and reported; __prob() after it is a no-op if the retry came
   back in problem state already. */
static int window(int (*fn)(void *), void *arg)
{
    unsigned char   key = PSWKEYNONE;
    unsigned        abend;
    int             rc;

    rc = __super(PSWKEY0, &key);
    if (rc) {
        wtof("TSTKEY0 __super rc=%d", rc);
        return rc;
    }
    try(fn, arg);
    abend = tryrc();
    __prob(key, 0);
    if (abend) {
        wtof("TSTKEY0 FAIL: abend %06X inside the window", abend);
        bad++;
    }
    return 0;
}

static int open_close(void *arg)
{
    FILE    *fp = fopen(MEMBER, "rb");

    *(int *)arg = (fp != 0);
    if (fp) fclose(fp);
    return 0;
}

static int first_getenv(void *arg)
{
    *(char **)arg = getenv("K0VAR");
    return 0;
}

static int first_stdout(void *arg)
{
    *(int *)arg = fputs("TSTKEY0 C: written inside the window\n", stdout);
    return 0;
}

static int open_keep(void *arg)
{
    *(FILE **)arg = fopen(MEMBER, "rb");
    return 0;
}

static int close_it(void *arg)
{
    if (*(FILE **)arg) fclose(*(FILE **)arg);
    return 0;
}

static int key8_open(int *ok)
{
    *ok = 0;
    open_close(ok);
    return 0;
}

static int key8_setenv(const char *name, const char *value, int *ok)
{
    *ok = setenv(name, value, 1) == 0;
    return 0;
}

static int key8_stdout(int *ok)
{
    *ok = fputs("TSTKEY0 C: written after the window\n", stdout) >= 0
          && fflush(stdout) == 0;
    return 0;
}

#define FILL 20         /* ARRAY_DEFAULT: the list grows at the 21st */

static int case_d(void)
{
    FILE    *held[FILL];
    FILE    *w = 0;
    int     ok;
    int     n = 0;
    int     i;

    for (i = 0; i < FILL; i++) {
        held[i] = fopen(MEMBER, "rb");
        if (held[i]) n++;
    }
    wtof("TSTKEY0 D: %d of %d files open in key 8", n, FILL);
    if (window(open_keep, &w)) return 12;
    wtof("TSTKEY0 D: the 21st fopen() inside the window %s",
         w ? "opened" : "FAILED");
    ok = 0;
    try(key8_open, &ok);
    want(ok && tryrc() == 0 && n == FILL && w,
         "D fopen() in key 8 after the list grew inside the window",
         tryrc());
    /* its FILE is key 0: it is closed where it was opened */
    if (window(close_it, &w)) return 12;
    for (i = 0; i < FILL; i++) if (held[i]) fclose(held[i]);

    wtof("TSTKEY0 VERDICT D failed=%d", bad);
    return bad ? 8 : 0;
}

int main(int argc, char **argv)
{
    int     ok;
    char    *val = 0;
    char    *v;

    if (!__isauth()) {
        wtof("TSTKEY0 FAIL: not APF authorized");
        return 12;
    }
    if (argc > 1 && argv[1][0] == 'D') return case_d();

    /* A - grt->grtfile */
    ok = 0;
    if (window(open_close, &ok)) return 12;
    wtof("TSTKEY0 A: fopen(%s) inside the window %s", MEMBER,
         ok ? "opened" : "FAILED");
    ok = 0;
    try(key8_open, &ok);
    want(ok && tryrc() == 0, "A fopen() in key 8 after the window",
         tryrc());

    /* B - the SYSENV environment */
    if (window(first_getenv, &val)) return 12;
    wtof("TSTKEY0 B: getenv(K0VAR) inside the window = %s",
         val ? val : "(null)");
    want(val && strcmp(val, "ONE") == 0, "B SYSENV loaded inside the window",
         0);
    ok = 0;
    try(key8_setenv, "K0NEW", "TWO", &ok);
    want(ok && tryrc() == 0, "B1 setenv() of a new name in key 8", tryrc());
    ok = 0;
    try(key8_setenv, "K0VAR", "THREE", &ok);
    want(ok && tryrc() == 0, "B2 setenv() of a SYSENV name in key 8",
         tryrc());
    v = getenv("K0NEW");
    val = getenv("K0VAR");
    want(v && strcmp(v, "TWO") == 0 && val && strcmp(val, "THREE") == 0,
         "B3 getenv() reads both back", 0);

    /* C - stdout */
    ok = 0;
    if (window(first_stdout, &ok)) return 12;
    ok = 0;
    try(key8_stdout, &ok);
    want(ok && tryrc() == 0, "C fputs()/fflush() to stdout in key 8",
         tryrc());

    wtof("TSTKEY0 VERDICT ABC failed=%d", bad);
    return bad ? 8 : 0;
}
