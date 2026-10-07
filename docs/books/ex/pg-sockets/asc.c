/* asc.c - EBCDIC/ASCII tables built from the compiler's own mapping */
#include <string.h>
#include "asc.h"

/* The printable ASCII characters, in ASCII order, from the blank
   (ASCII X'20') to the tilde (ASCII X'7E').  The compiler stores each
   one as its EBCDIC byte, so position i holds the EBCDIC form of the
   ASCII character X'20' + i. */
static const char printable[] =
    " !\"#$%&'()*+,-./0123456789:;<=>?"
    "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
    "`abcdefghijklmnopqrstuvwxyz{|}~";

#define ASCII_BLANK 0x20        /* ASCII code of printable[0] */

void asc_init(ASCTAB *t)
{
    unsigned char qmark = ASCII_BLANK + (strchr(printable, '?') - printable);
    int           i;

    /* bytes without a counterpart become a question mark */
    memset(t->toasc, qmark, sizeof(t->toasc));
    memset(t->toebc, '?', sizeof(t->toebc));

    for (i = 0; printable[i]; i++) {
        t->toasc[(unsigned char)printable[i]] = ASCII_BLANK + i;
        t->toebc[ASCII_BLANK + i] = printable[i];
    }

    /* the control characters a line protocol needs */
    t->toasc[(unsigned char)'\n'] = 0x0A;   t->toebc[0x0A] = '\n';
    t->toasc[(unsigned char)'\r'] = 0x0D;   t->toebc[0x0D] = '\r';
    t->toasc[(unsigned char)'\t'] = 0x09;   t->toebc[0x09] = '\t';
}

void asc_out(const ASCTAB *t, char *buf, int len)
{
    while (len-- > 0) {
        *buf = t->toasc[(unsigned char)*buf];
        buf++;
    }
}

void asc_in(const ASCTAB *t, char *buf, int len)
{
    while (len-- > 0) {
        *buf = t->toebc[(unsigned char)*buf];
        buf++;
    }
}
