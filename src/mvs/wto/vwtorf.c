/* VWTOF.C */
#define CLIB_C
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include "mvs/crt.h"
#include "mvs/wto.h"
#include "stddef.h"
#include "ibm/mvs/ihacde.h"

void
vwtorf(char *reply, unsigned replymax, const char *text, va_list tList)
{
    int     len;
    char    *p;
    char    *s;
    char    *last;
    char    buf[4096];

    len = vsnprintf(buf, sizeof(buf), text, tList);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    buf[len] = 0;

    /* every line but the last is an ordinary message, the last one is
       the WTOR.  A text ending in a newline used to write all its lines
       with wto() and issue no WTOR at all, and the rest of the buffer was
       moved left with an overlapping strcpy() (#404). */
    s = buf;
    last = NULL;
    while ((p = strchr(s, '\n')) != NULL) {
        *p = 0;
        if (last) wto(last);
        last = s;
        s = p + 1;
    }
    if (*s) {
        if (last) wto(last);
        last = s;
    }
    wtor(reply, replymax, last ? last : s);
}
