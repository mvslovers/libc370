/* @@DBLCVT.C */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <stddef.h>

/*
   result has rsize bytes, and nothing is written past them (#222).
   What does not fit is cut off: the NUL always stays, an exponent
   before any fraction digit, digits before padding.

   alt is printf's # flag: %e and %f keep the decimal point with
   precision 0, %g keeps its trailing zeros (#578).  __dblcvt() is
   __dblcvx() without it.

   The work is split into one function per step (#578: the single
   function had a cognitive complexity of 105).  b is the value scaled
   into [1,10) and exp its decimal exponent throughout; format says how
   it is printed:
         -1 => below 1, we need leading digits
          0 => exponent form
         +1 => digits before the point
*/

/* rounding past this fractional digit changes no digit; see roundb() */
#define DBLCVT_MAXRND 17

/* the result being built: n characters so far, digits up to dlim */
typedef struct dblout {
    char    *result;
    size_t  n;
    size_t  lim;            /* rsize - 1: room for the NUL */
    size_t  dlim;           /* where the digits stop */
} DBLOUT;

/* append one character while there is room below dlim */
static void
put(DBLOUT *o, char c)
{
    if (o->n < o->dlim) o->result[o->n++] = c;
}

/* the next digit of b after digit i, and i becomes it */
static void
nextdigit(DBLOUT *o, double *b, int *i)
{
    *b = (*b - *i) * 10.0;
    *i = (int)*b;
    put(o, (char)('0' + *i % 10));
}

/* the powers of ten a double holds exactly: 5**22 has 52 bits, and an
   HFP fraction keeps at least 53 */
static const double pow10[23] = {
    1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
    1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22
};

/* scale b into [1,10) and return the decimal exponent.
   The exponent is found by comparing with the exact powers, and b is then
   divided or multiplied by them, at most four times for the whole HFP
   range.  It was one division or multiplication by 10 per decimal
   exponent, up to 75: each truncates, the error built up in one
   direction, and the 15th digit of 1e60 or 1e-30 was off (#225). */
static int
scale(double *b)
{
    double  v   = *b;
    int     exp = 0;
    int     k;

    if (v == 0.0) return 0;

    /* the exponent: whole steps of 22, then the rest.  HFP stays within
       10**+-79; the bound only keeps a host's infinity out of a loop */
    while (v >= 1e22 && exp < 330) {
        v = v / 1e22;
        exp += 22;
    }
    while (v < 1.0 && exp > -330) {
        v = v * 1e22;
        exp -= 22;
    }
    for (k = 22; k > 0 && v < pow10[k]; k--) ;
    exp += k;

    /* b / 10**exp from b itself, with as few operations as there are */
    v = *b;
    for (k = exp; k > 22; k -= 22) v = v / 1e22;
    if (k > 0) v = v / pow10[k];
    for (k = -exp; k > 22; k -= 22) v = v * 1e22;
    if (k > 0 && exp < 0) v = v * pow10[k];

    /* a truncated operation can leave it just outside */
    if (v >= 10.0) {
        v = v / 10.0;
        exp++;
    }
    else if (v < 1.0) {
        v = v * 10.0;
        exp--;
    }
    *b = v;
    return exp;
}

/*
  %g, C99 7.19.6.1: the precision P counts significant digits (0 is
  taken as 1).  With X the exponent the e style would have, after
  rounding to P digits, it is %f with precision P-1-X when P > X >= -4,
  else %e with precision P-1.  It used to take P as the number of
  decimals, decide on the unrounded exponent and keep P decimals in the
  e style: %.3g of 123.456 printed 123.456 (#220).  Returns 'e' or 'f'
  and sets the precision of that style.
*/
static char
gstyle(double b, int exp, int *nprecision)
{
    int     p = (*nprecision > 0) ? *nprecision : 1;
    int     x = exp;
    double  round = 0.5;
    int     i;

    if (b != 0.0) {
        for (i = 0; i < p - 1 && i < DBLCVT_MAXRND; i++) {
            round = round / 10.0;
        }
        if (b + round >= 10.0) {
            x++;
        }
    }
    if (x < p && x >= -4) {
        *nprecision = p - 1 - x;
        return 'f';
    }
    *nprecision = p - 1;
    return 'e';
}

/*
  Round: add half a unit of the last digit printed, which is fractional
  digit j of b.  j only has to be bounded so the addend stays
  representable.  It must not be DBL_MANT_DIG - on S/370 that counts hex
  digits (14), and capping there added a fixed 5e-15 to every value
  printed with 14 or more digits (#209).  From digit DBLCVT_MAXRND on,
  the addend is below half an ulp of b in [1,10) (HFP: 16**-13, about
  2.2e-16) and changes no digit.  Zero is not rounded: no ulp absorbs
  the addend there, and it would print as a stray 5 (#209).
*/
static void
roundb(double *b, int *exp, int *format, int nprecision)
{
    int     j = (*format == 0) ? nprecision : *exp + nprecision;
    double  round = 0.5;
    int     i = 0;

    if (j > DBLCVT_MAXRND) {
        j = DBLCVT_MAXRND;
    }
    if (j >= 0) {
        if (*b == 0.0) return;
        while (i++ < j) {
            round = round / 10.0;
        }
        *b = *b + round;
        if (*b >= 10.0) {
            *b = *b / 10.0;
            *exp = *exp + 1;
        }
        return;
    }
    /* %f of a number below 1 whose first digit lies past the last one
       printed: it rounds to one unit of the last place when the digit
       after it, b's first, is 5 or more (j == -1), else to zero.  This
       added 5 to b whatever its first digit, so %.3f of 0.000123 printed
       0.0006, and j < -1 printed digits past the precision: %.1f of
       0.001 gave 0.001 (#220) */
    if (j == -1 && *b >= 5.0) {
        *b = 1.0;
        *exp = *exp + 1;
    }
    else {
        *b = 0.0;
        *exp = 0;
        *format = 1;
    }
}

/* below 1: "0.", the leading zeros, then the digits */
static void
fbelow1(DBLOUT *o, double b, int exp, int nprecision)
{
    int i;
    int pdigits;

    put(o, '0');
    put(o, '.');
    while (++exp) {
        --nprecision;
        put(o, '0');
    }
    i = (int)b;
    --nprecision;
    put(o, (char)('0' + i % 10));

    pdigits = nprecision;
    while (pdigits-- > 0 && o->n < o->dlim) {
        nextdigit(o, &b, &i);
    }
}

/* 1 and up: the integer digits, the point, the decimals */
static void
fabove1(DBLOUT *o, double b, int exp, int nprecision, int alt)
{
    int i = (int)b;
    int pdigits;

    put(o, (char)('0' + i % 10));
    if (alt && exp == 0 && nprecision == 0) {
        put(o, '.');                /* %#.0f keeps its point */
    }
    nprecision = nprecision + exp;
    pdigits = nprecision;

    while (pdigits-- > 0 && o->n < o->dlim) {
        if ((nprecision - pdigits - 1) == exp) {
            put(o, '.');
        }
        nextdigit(o, &b, &i);
    }
    if (alt && exp > 0 && nprecision == exp) {
        put(o, '.');                /* %#.0f of 10 and up */
    }
}

/* exponent form: one digit, the point, the decimals */
static void
emant(DBLOUT *o, double b, int nprecision, int alt)
{
    int i = (int)b;
    int pdigits = nprecision;

    put(o, (char)('0' + i % 10));
    if (nprecision > 0 || alt) {
        /* no point with precision 0, C99 7.19.6.1: %.0e of 1 is 1e+00,
           it printed 1.E+00 (#578) */
        put(o, '.');
    }
    while (pdigits-- > 0 && o->n < o->dlim) {
        nextdigit(o, &b, &i);
    }
}

/* %g drops the trailing zeros of the fraction, and the point with them,
   in either style - before the exponent (#221) */
static void
gstrip(DBLOUT *o)
{
    char    *p = strchr(o->result, '.');
    char    *q;

    if (p != NULL) {
        q = o->result + strlen(o->result) - 1;
        while (q > p && *q == '0') {
            *q-- = '\0';
        }
        if (q == p) {
            *q = '\0';
        }
    }
    o->n = strlen(o->result);
}

/* the exponent: e for %e and %g, E for %E and %G (#221) */
static void
expo(DBLOUT *o, int exp, char cnvtype)
{
    o->dlim = o->lim;
    put(o, (cnvtype == 'e' || cnvtype == 'g') ? 'e' : 'E');
    if (exp < 0) {
        exp = -exp;
        put(o, '-');
    }
    else {
        put(o, '+');
    }
    put(o, (char)('0' + (exp / 10) % 10));
    put(o, (char)('0' + exp % 10));
    o->result[o->n] = '\0';
}

/* blanks in front up to nwidth - only as far as the buffer goes */
static void
padleft(DBLOUT *o, size_t nwidth)
{
    size_t  pad;

    if (nwidth > o->n) {
        pad = nwidth - o->n;
        if (pad > o->lim - o->n) {
            pad = o->lim - o->n;
        }
        memmove(o->result + pad, o->result, o->n + 1);
        while (pad > 0) {
            o->result[--pad] = ' ';
        }
    }
}

void
__dblcvx(double num, char cnvtype, size_t nwidth, int nprecision, int alt,
         char *result, size_t rsize)
{
    DBLOUT  o;
    double  b = (num < 0) ? -num : num;
    int     exp;
    int     format;
    char    style = (cnvtype == 'e' || cnvtype == 'E') ? 'e' : 'f';

    if (rsize == 0) {
        return;
    }
    o.result = result;
    o.n = 0;
    o.lim = rsize - 1;

    exp = scale(&b);
    if (cnvtype == 'g' || cnvtype == 'G') {
        style = gstyle(b, exp, &nprecision);
    }
    format = (style == 'e') ? 0 : (exp >= 0) ? 1 : -1;

    roundb(&b, &exp, &format, nprecision);
    if (format == -1 && exp >= 0) {
        format = 1;
    }

    /* A positive number gets no sign position; the digits of an e-style
       number stop four bytes short, so its exponent always fits. */
    o.dlim = o.lim;
    if (format == 0) {
        o.dlim = (o.lim > 4) ? o.lim - 4 : 0;
    }
    if (num < 0) {
        put(&o, '-');
    }

    if (format == -1) {
        fbelow1(&o, b, exp, nprecision);
    }
    else if (format == 1) {
        fabove1(&o, b, exp, nprecision, alt);
    }
    else {
        emant(&o, b, nprecision, alt);
    }
    result[o.n] = '\0';

    if ((cnvtype == 'G' || cnvtype == 'g') && !alt) {
        gstrip(&o);
    }
    if (format == 0) {
        expo(&o, exp, cnvtype);
    }
    padleft(&o, nwidth);
}

void
__dblcvt(double num, char cnvtype, size_t nwidth, int nprecision,
         char *result, size_t rsize)
{
    __dblcvx(num, cnvtype, nwidth, nprecision, 0, result, rsize);
}
