/*
 * tstwto.c - libc370 #404: the WTO helpers must not write outside their
 * buffers or into the caller's text.
 *
 * ISSUE #404: wto() moved the rest of the caller's string left after
 * every 124-byte message (a literal in protected storage was written
 * to); vwtof()/vwtorf() moved their own buffer with an overlapping
 * strcpy(); vwtorf() issued no WTOR for a text ending in a newline;
 * wtodump() ran past its 80-byte lines for a chunk above 35 or of 0 or
 * less; wtodumpf() past its 80-byte title.
 *
 * This #includes the real wto.c, vwtof.c, vwtorf.c, wtorf.c, wtodump.c
 * and wtodumpf.c under AddressSanitizer.  wtoline() and wtor() are stand-ins
 * that record what would go to the console; wtof() writes through the
 * real vwtof().  wtor()'s own change (the reply terminated inside
 * replymax) is SVC 35 and is not tested here.
 *
 * BUILD / RUN (host, from test/host):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra -fsanitize=address \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R -o t tstwto.c && ./t
 *
 * RED, 2026-10-18, against main: ASan memcpy-param-overlap in wto() on
 * the first case - the shift of the rest of the caller's own string,
 * here a 300-character literal.  main's wtodump.c alone, without
 * ASan and with the overrunning calls taken out: the chunk-6 check fails
 * (the last hexadecimal digit of the line is cut off).
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "../../src/mvs/wto/wto.c"
#include "../../src/mvs/wto/vwtof.c"
#include "../../src/mvs/wto/vwtorf.c"
#include "../../src/mvs/wto/wtorf.c"
#include "../../src/mvs/wto/wtodump.c"
#include "../../src/mvs/wto/wtodumpf.c"

/* ---- what went to the console ----------------------------------------- */
static char lines[64][130];
static int  nlines;
static char wtortext[130];
static int  nwtor;

void wtoline(const WTOMSG *msg)
{
    int n = msg->len - 4;

    if (nlines < 64) {
        memcpy(lines[nlines], msg->buf, n);
        lines[nlines][n] = 0;
    }
    nlines++;
}

void wtor(char *reply, unsigned replymax, const char *text)
{
    (void)reply; (void)replymax;
    snprintf(wtortext, sizeof(wtortext), "%s", text);
    nwtor++;
}

void wtof(const char *text, ...)
{
    va_list ap;

    va_start(ap, text);
    vwtof(text, ap);
    va_end(ap);
}

/* libc370's ctype is table-driven (isgraph() in wtodump): ASCII here */
static unsigned short isbuf_tbl[256];
unsigned short *__isbuf = isbuf_tbl;

static void ctype_init(void)
{
    int c;

    for (c = 0x21; c < 0x7f; c++) isbuf_tbl[c] = 0xFFFF;
}

static void reset(void) { nlines = 0; nwtor = 0; wtortext[0] = 0; }

/* ---- harness ------------------------------------------------------------ */
static int run = 0, failed = 0;

static void check(int cond, const char *what)
{
    run++;
    if (!cond) {
        failed++;
        printf("FAIL: %s\n", what);
    }
}

static const char long300[] =
    "ALPHA BRAVO CHARLIE DELTA ECHO FOXTROT GOLF HOTEL INDIA JULIETT KILO "
    "LIMA MIKE NOVEMBER OSCAR PAPA QUEBEC ROMEO SIERRA TANGO UNIFORM VICTOR "
    "WHISKEY XRAY YANKEE ZULU ALPHA BRAVO CHARLIE DELTA ECHO FOXTROT GOLF "
    "HOTEL INDIA JULIETT KILO LIMA MIKE NOVEMBER OSCAR PAPA QUEBEC ROMEO END";

int main(void)
{
    char    joined[400];
    char    area[100];
    char    title[200];
    int     i;

    ctype_init();

    /* wto(): a literal longer than one message is written, and kept */
    reset();
    wto(long300);
    joined[0] = 0;
    for (i = 0; i < nlines && i < 64; i++) strcat(joined, lines[i]);
    check(nlines >= 3, "a 300-character text is several messages");
    check(strcmp(joined, long300) == 0, "the messages are the text, in order");
    for (i = 0; i < nlines && i < 64; i++)
        if (strlen(lines[i]) > 125) check(0, "no message longer than 125");

    reset();
    wto("");
    check(nlines == 1 && lines[0][0] == 0, "wto(\"\") writes one empty message");

    /* vwtof() through wtof(): lines */
    reset();
    wtof("A\nB\n");
    check(nlines == 2 && !strcmp(lines[0], "A") && !strcmp(lines[1], "B"),
          "wtof(\"A\\nB\\n\"): two messages, no empty one");
    reset();
    wtof("A\n\nB");
    check(nlines == 3 && lines[1][0] == 0, "wtof(\"A\\n\\nB\"): an empty message between");
    reset();
    wtof("%s", "");
    check(nlines == 1, "wtof(\"\"): one empty message");

    /* vwtorf(): the last line is the WTOR, also with a trailing newline */
    reset();
    wtorf(NULL, 0, "Q1\nQ2\n");
    check(nlines == 1 && !strcmp(lines[0], "Q1") && nwtor == 1 && !strcmp(wtortext, "Q2"),
          "wtorf(\"Q1\\nQ2\\n\"): Q1 a message, Q2 the WTOR");
    reset();
    wtorf(NULL, 0, "ONLY");
    check(nlines == 0 && nwtor == 1 && !strcmp(wtortext, "ONLY"), "wtorf(\"ONLY\"): the WTOR");
    reset();
    wtorf(NULL, 0, "A\nB");
    check(nlines == 1 && nwtor == 1 && !strcmp(wtortext, "B"), "wtorf(\"A\\nB\"): B the WTOR");

    /* wtodump(): chunks outside 1-32 */
    for (i = 0; i < (int)sizeof(area); i++) area[i] = (char)i;
    reset();
    wtodump("T", area, sizeof(area), 36);
    check(nlines > 1, "wtodump() chunk 36 runs");
    reset();
    wtodump("T", area, sizeof(area), 0);
    check(nlines > 1, "wtodump() chunk 0 runs");
    reset();
    wtodump("T", area, sizeof(area), -5);
    check(nlines > 1, "wtodump() chunk -5 runs");
    reset();
    wtodump("T", area, 6, 6);
    check(nlines == 2 && strstr(lines[1], " 00010203 0405 :") != NULL,
          "wtodump() chunk 6 shows all 12 hex digits");

    /* wtodumpf(): a title longer than 79 */
    for (i = 0; i < (int)sizeof(title) - 1; i++) title[i] = 'T';
    title[sizeof(title) - 1] = 0;
    reset();
    wtodumpf(area, 16, "%s", title);
    check(nlines >= 2, "wtodumpf() with a 199-character title runs");

    printf("tstwto: %d checks, %d failed\n", run, failed);
    return failed ? 1 : 0;
}
