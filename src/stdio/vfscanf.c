/* VFSCANF.C */
#include "src/internal/fileio.h"
#include <stdio.h>
#include <stdarg.h>

/* C99 7.19.6.9 (#338) */
int
vfscanf(FILE *stream, const char *format, va_list arg)
{
    return (vvscanf(format, arg, stream, NULL));
}
