/* VSSCANF.C */
#include "src/internal/fileio.h"
#include <stdio.h>
#include <stdarg.h>

/* C99 7.19.6.12 (#338) */
int
vsscanf(const char *s, const char *format, va_list arg)
{
    return (vvscanf(format, arg, NULL, s));
}
