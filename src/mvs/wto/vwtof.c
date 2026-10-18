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
vwtof(const char *text, va_list tList)
{
    int     len;
    char    *p;
    char    *s;
    char    buf[4096];

    len = vsnprintf(buf, sizeof(buf), text, tList);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    buf[len] = 0;

    /* one message per line.  The rest of the buffer used to be moved
       left with an overlapping strcpy() after every line (#404); a
       pointer walks it now.  A newline at the end makes no empty
       message, a text without one is written even when empty. */
    s = buf;
    while ((p = strchr(s, '\n')) != NULL) {
        *p = 0;
        wto(s);
        s = p + 1;
    }
    if (*s || s == buf) wto(s);
}
