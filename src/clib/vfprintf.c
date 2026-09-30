/* VFPRINTF.C */
#include "src/internal/fileio.h"
#include <stdio.h>

int
vfprintf(FILE *stream, const char *format, va_list arg)
{
    int ret;

    ret = vvprintf(format, arg, stream, NULL);

    return (ret);
}
