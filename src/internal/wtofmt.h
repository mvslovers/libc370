#ifndef SRC_INTERNAL_WTOFMT_H
#define SRC_INTERNAL_WTOFMT_H
/* src/internal/wtofmt.h - internal: compose a console message without the
** printf family (#564).
**
** A library diagnostic written with wtof() links vsnprintf() and with it the
** whole of printf: @@examin, @@dblcvt, the 64-bit helpers.  Diagnostics on
** paths every program links - malloc() out of storage, the task anchor not
** found, an ESTAE without a routine - made that the price of an empty
** main().  These append text, hex and decimal to a buffer the caller then
** hands to wto().  Each keeps buf NUL-terminated and stops at max - 1.
** One copy in libc.a (src/mvs/wto/@@wtofmt.c), not inline in every caller.
*/

void __wfstr(char *buf, int *i, int max, const char *s);
void __wfhex(char *buf, int *i, int max, unsigned v, int digits);
void __wfdec(char *buf, int *i, int max, unsigned v);

#define wtofmt_str  __wfstr
#define wtofmt_hex  __wfhex     /* the low `digits` hex digits of v */
#define wtofmt_dec  __wfdec

#endif /* SRC_INTERNAL_WTOFMT_H */
