/* WTO.C */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

typedef struct wtomsg {
    short   len;
    short   mcsflags;
    char    buf[128];
} WTOMSG;

extern void wtoline(const WTOMSG *msg);

/* The text is walked with a pointer.  It used to be shifted left in the
   caller's buffer after every 124-byte message, so the caller got its
   string back changed, and a literal in protected storage was written to
   (#404). */
void
wto(const char *buf)
{
    const char  *s  = buf;
    int         len = strlen(s);
    WTOMSG      msg;

    do {
        if (len <= 124) {
            msg.len = len + 4;
            msg.mcsflags = 0;
            if (len > 0) memcpy(msg.buf, s, len);
            len = 0;
        }
        else {
            const char *p = &s[124];

            while(*p != ' ' && p > s) p--;

            if (p==s) {
                p = &s[124];    /* no spaces, use 124 characters */
            }
            else if (*p==' ') {
                p++; /* include last space */
            }

            msg.len = (int)(p-s);       /* length of message segment */
            msg.mcsflags = 0;
            memcpy(msg.buf, s, msg.len);
            msg.buf[msg.len] = 0;
            /* go on after the segment */
            len -= msg.len;
            s = p;
            msg.len += 4;
        }
        wtoline(&msg);
    } while(len > 0);
}
