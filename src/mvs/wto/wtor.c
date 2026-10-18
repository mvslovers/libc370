/* WTO.C */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

typedef struct wtormsg {
    union {
        unsigned    reply;
        struct {
            char    reply_len;
            char    reply_addr[3];
        } __attribute__((packed));
    };
    unsigned        *ecb;
    unsigned short  text_len;
    unsigned short  mcs_flags;
    char            text[122];
} WTORMSG;

void
wtor(char *reply, unsigned replymax, const char *text)
{
    unsigned    ecb     = 0;
    unsigned    len     = 0;
    WTORMSG     msg     = {0};

    if (text) {
        len = strlen(text);
        if (len > sizeof(msg.text)) len = sizeof(msg.text);
    }

    /* replymax is the size of the reply buffer.  The reply always ends
       with a null character inside it: the buffer is cleared, and MVS is
       asked for one character less than its size, at most 119.  The
       reply was not terminated, and the caller had to clear a buffer one
       byte longer than replymax (#404). */
    if (reply && replymax) {
        if (replymax > 120) replymax = 120;
        memset(reply, 0, replymax);
        if (replymax > 1) {
            msg.reply = (unsigned)reply;
            msg.reply_len = replymax - 1;
            msg.ecb = &ecb;
        }
    }

    if (len) {
        msg.text_len = sizeof(msg.text_len) + sizeof(msg.mcs_flags) + len;
        memcpy(msg.text, text, len);
    }

    __asm__("LR\t1,%0\n\t"
            "SVC\t35" : : "r"(&msg) : "0", "1", "14", "15", "memory");

    if (msg.ecb) {
        __asm__("WAIT\t1,ECB=(%0)" : : "r"(msg.ecb) : "0", "1", "14", "15", "memory");
    }
}
