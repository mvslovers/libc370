#ifndef CLIBTSO_H
#define CLIBTSO_H
#include <sys/_cc370.h>

/* tsocmd() - link to pgm as TSO command processor */
int tsocmd(const char *pgm, const char *buf);

/* tsocmdf() - link to pgm as TSO command processor, format buffer */
int tsocmdf(const char *pgm, const char *fmt, ...);

#endif
