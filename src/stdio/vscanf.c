/* VSCANF.C */
#include "src/internal/fileio.h"
#include <stdio.h>
#include <stdarg.h>

/* C99 7.19.6.11 (#338) */
int
vscanf(const char *format, va_list arg)
{
    return (vvscanf(format, arg, stdin, NULL));
}
