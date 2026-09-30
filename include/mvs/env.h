#ifndef CLIBENV_H
#define CLIBENV_H

#include <stdlib.h>

typedef struct __envvar __ENVVAR;
struct __envvar {
    char    *name;
    char    *value;
    char    buf[2];
};

extern __ENVVAR **__envvar;
extern int      __envsiz;

extern char *   __findenv(const char *name, int *index, int nocase);

extern char *   getenv(const char *name);

extern char *   getenvi(const char *name);

extern int      setenvi(const char *name, int value, int rewrite);

/* load "name=value" lines from a data set into the environment; fn is a
   name fopen() understands, e.g. "dd:SYSENV".  MVS sequence numbers in the
   last 8 columns and "*"/"#" comment lines are stripped.  0 = loaded,
   non-zero = the data set could not be opened. */
extern int      loadenv(const char *fn);

#endif
