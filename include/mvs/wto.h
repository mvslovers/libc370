#ifndef CLIBWTO_H
#define CLIBWTO_H
#include <sys/_cc370.h>
#include <stdarg.h>
#include <s370/savearea.h>

/* wto() does not change buf (#404) */
extern void wto(const char *buf);

extern void vwtof(const char *text, va_list tList)
                                        __attribute__((format(printf, 1, 0)));

/* the format attributes let the compiler check the arguments against the
   format (#68) */
extern void wtof(const char *text, ...)
                                        __attribute__((format(printf, 1, 2)));

extern void wtodumpf(void *buf, int len, const char *fmt, ...)
                                        __attribute__((format(printf, 3, 4)));

extern void wtodump(const char *title, void *varea, int size, int chunk);

extern void wto_traceback(SAVEAREA *sa) asm("@@WTOTB");

/* write to operator with reply */
extern void wtorf(char *reply, unsigned replymax, const char *text, ...)
                                        __attribute__((format(printf, 3, 4)));
extern void vwtorf(char *reply, unsigned replymax, const char *text, va_list tList)
                                        __attribute__((format(printf, 3, 0)));
extern void wtor(char *reply, unsigned replymax, const char *text);

#endif
