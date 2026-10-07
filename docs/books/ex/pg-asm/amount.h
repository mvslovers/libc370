#ifndef AMOUNT_H
#define AMOUNT_H
#include <stddef.h>

char *amt_format(char *buf, size_t size, long cents);   /* AMT@FORM */
long  amt_parse(const char *s);                         /* AMT@PARS */

#endif
