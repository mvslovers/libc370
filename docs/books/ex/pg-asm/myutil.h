#ifndef MYUTIL_H
#define MYUTIL_H

int pd2int(const void *pd, int len)                        asm("PD2INT");
int eachchr(const char *s, int (*fn)(int c, void *arg),
            void *arg)                                     asm("EACHCHR");

#endif
