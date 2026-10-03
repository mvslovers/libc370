/* VPRINTF.C */
#include "src/internal/fileio.h"
#include <stdio.h>
#include <stdarg.h>

/* C99 7.19.6.10 (#338) */
int
vprintf(const char *format, va_list arg)
{
    return (vfprintf(stdout, format, arg));
}
