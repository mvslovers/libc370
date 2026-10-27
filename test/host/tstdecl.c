/*
 * tstdecl.c - libc370 #420: public declarations match their definitions.
 *
 * ISSUE #420: on_exit() and ikjct441() were defined and exported by
 * libc.a with no declaration in include/, and asc2ebc / ebc2asc were
 * defined without including the header that declares them, so a
 * different signature would have compiled unnoticed.
 *
 * Each function is first used through its header's declaration, then the
 * defining file is #included: a mismatch is a "conflicting types" error.
 *
 * BUILD / RUN (host, from test/host):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra -Werror \
 *        -Wno-int-to-pointer-cast -Wno-asm-operand-widths -U__LP64__ \
 *        -D'__asm__(...)=' \
 *        -D'asm(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R -o t tstdecl.c && ./t
 *
 * RED, 2026-10-27, against main 338bc5a: "'on_exit' undeclared",
 * "'ikjct441' undeclared".
 *
 * RC: 0 = it compiled and the checks passed, 1 = a check failed.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mvs/tso.h>
#include <mvs/jes2.h>
#include "src/net/dyn75/x75.h"

static int (*p_on_exit)(void (*)(int, void *), void *) = on_exit;
static int (*p_ikjct441)(int, const char *, char *, int) = ikjct441;
static int (*p_jesirput)(VSFILE *, const char[80]) = jesirput;

#include "../../src/stdlib/on@exit.c"
#include "../../src/mvs/tso/ikjct441.c"
#include "../../src/mvs/jes2/jesirput.c"
#include "../../src/stdlib/@@userex.c"
#include "../../src/net/dyn75/asc2ebc.c"
#include "../../src/net/dyn75/ebc2asc.c"

static int bad;

/* stand-ins for what the defining files call, so the test links on the host */
CLIBGRT *__grtget(void) { return NULL; }
int lock(void *thing, int read) { (void)thing; (void)read; return 0; }
int unlock(void *thing, int read) { (void)thing; (void)read; return 0; }
int arrayadd(void *a, void *i) { (void)a; (void)i; return 0; }
unsigned arraycount(void *a) { (void)a; return 0; }
void *arraydel(void *a, unsigned i) { (void)a; (void)i; return NULL; }
char *strcpyp(char *t, int tl, const void *s, int pad)
{ (void)tl; (void)s; (void)pad; return t; }

static void check(const char *what, int ok)
{
    printf("  %s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) bad++;
}

int main(void)
{
    check("on_exit declared as defined", p_on_exit == on_exit);
    check("ikjct441 declared as defined", p_ikjct441 == ikjct441);
    check("jesirput declared as defined", p_jesirput == jesirput);
    check("__userExit has __NATEXIT slots",
          sizeof(__userExit) / sizeof(__userExit[0]) == __NATEXIT);
    check("asc2ebc maps 'A' to 0xC1", asc2ebc[0x41] == 0xC1);
    check("ebc2asc maps 0xC1 to 'A'", ebc2asc[0xC1] == 0x41);
    check("ECODE_TSVERETR is 1", ECODE_TSVERETR == 1);
    printf("tstdecl: %s\n", bad ? "FAILED" : "passed");
    return bad ? 1 : 0;
}
