#ifndef CLIBTSO_H
#define CLIBTSO_H
#include <sys/_cc370.h>

/* tsocmd() - link to pgm as TSO command processor */
int tsocmd(const char *pgm, const char *buf);

/* tsocmdf() - link to pgm as TSO command processor, format buffer */
int tsocmdf(const char *pgm, const char *fmt, ...);

/* ECODE values of ikjct441() */
#define ECODE_TSVERETR    1
#define ECODE_TSVEUPDT    2
#define ECODE_TSVELOC     3
#define ECODE_TSVERSVD    4
#define ECODE_TSVNOIMP    18

/* ikjct441() - TSO CLIST variable access via IKJCT441 (z/OS; not for MVS
   3.8).  Returns the IKJCT441 return code. */
int ikjct441(int ecode, const char *name, char *value, int value_size);

#endif
