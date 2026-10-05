/*
 * tstpfargcase.h - the cases of test/host/tstpfarg.c and
 * test/mvs/tstpfarg.c (libc370 #383): every conversion printf does not
 * handle in its fast path takes its argument off the list.
 *
 * Before #383 the printf engine printed nothing for %c with a width or a
 * flag, %n with a length modifier, %a and %A, and took no argument for
 * them, so every later conversion read the argument meant for the one
 * before it.
 *
 * C99 7.19.6.1: %c pads to the width like any conversion; %n stores the
 * count through a pointer whose type the length modifier names (hh, h,
 * l, ll, j, z, t); a conversion with no defined meaning is undefined, and
 * libc370 prints it as it stands.
 *
 * The includer supplies CHECK(cond, msg), <stdio.h>, <string.h> and
 * <stdarg.h>, and may define PFARG_SPRINTF to run each case through
 * vsprintf() (the vvprintf() path) as well as vsnprintf().
 */

static void pfa(const char *want, const char *f, ...)
{
    char b[64];
    va_list ap;
    int r;

    va_start(ap, f);
    r = vsnprintf(b, sizeof(b), f, ap);
    va_end(ap);
    CHECK(strcmp(b, want) == 0 && r == (int)strlen(want), f);
    if (strcmp(b, want) != 0 || r != (int)strlen(want))
        printf("        snprintf got \"%s\" (%d), want \"%s\"\n", b, r, want);
#ifdef PFARG_SPRINTF
    va_start(ap, f);
    r = vsprintf(b, f, ap);
    va_end(ap);
    CHECK(strcmp(b, want) == 0 && r == (int)strlen(want), f);
    if (strcmp(b, want) != 0 || r != (int)strlen(want))
        printf("        sprintf got \"%s\" (%d), want \"%s\"\n", b, r, want);
#endif
}

/* %n with a length modifier: the count lands in an object of the named
   width, between two guard bytes that must survive, and the argument
   after it is still the right one */
static void pfn(void)
{
    char b[32];
    int r;
    struct { char g1; signed char v; char g2; } hh = { 'G', -1, 'G' };
    struct { char g1; short v; char g2; } h = { 'G', -1, 'G' };
    struct { char g1; long v; char g2; } l = { 'G', -1, 'G' };
    struct { char g1; long long v; char g2; } ll = { 'G', -1, 'G' };
    struct { char g1; long long v; char g2; } j = { 'G', -1, 'G' };
    struct { char g1; long v; char g2; } z = { 'G', -1, 'G' };
    struct { char g1; long v; char g2; } t = { 'G', -1, 'G' };

    r = snprintf(b, sizeof(b), "abc%hhn|%d", &hh.v, 42);
    CHECK(r == 6 && strcmp(b, "abc|42") == 0 && hh.v == 3
          && hh.g1 == 'G' && hh.g2 == 'G', "%hhn stores a char, next arg intact");
    r = snprintf(b, sizeof(b), "abc%hn|%d", &h.v, 42);
    CHECK(r == 6 && strcmp(b, "abc|42") == 0 && h.v == 3
          && h.g1 == 'G' && h.g2 == 'G', "%hn stores a short, next arg intact");
    r = snprintf(b, sizeof(b), "abc%ln|%d", &l.v, 42);
    CHECK(r == 6 && strcmp(b, "abc|42") == 0 && l.v == 3
          && l.g1 == 'G' && l.g2 == 'G', "%ln stores a long, next arg intact");
    r = snprintf(b, sizeof(b), "abc%lln|%d", &ll.v, 42);
    CHECK(r == 6 && strcmp(b, "abc|42") == 0 && ll.v == 3
          && ll.g1 == 'G' && ll.g2 == 'G',
          "%lln stores a long long (both words), next arg intact");
    r = snprintf(b, sizeof(b), "abc%jn|%d", &j.v, 42);
    CHECK(r == 6 && strcmp(b, "abc|42") == 0 && j.v == 3
          && j.g1 == 'G' && j.g2 == 'G', "%jn stores an intmax_t, next arg intact");
    r = snprintf(b, sizeof(b), "abc%zn|%d", &z.v, 42);
    CHECK(r == 6 && strcmp(b, "abc|42") == 0 && z.v == 3
          && z.g1 == 'G' && z.g2 == 'G', "%zn stores a size_t-sized int, next arg intact");
    r = snprintf(b, sizeof(b), "abc%tn|%d", &t.v, 42);
    CHECK(r == 6 && strcmp(b, "abc|42") == 0 && t.v == 3
          && t.g1 == 'G' && t.g2 == 'G', "%tn stores a ptrdiff_t, next arg intact");
    /* the count includes what a width-padded conversion before it wrote */
    r = snprintf(b, sizeof(b), "%5c%hn|%d", 'x', &h.v, 42);
    CHECK(r == 8 && strcmp(b, "    x|42") == 0 && h.v == 5,
          "%hn after %5c counts the padding");
}

static void pfarg_cases(void)
{
    pfa("    x|42", "%5c|%d", 'x', 42);
    pfa("x  |42", "%-3c|%d", 'x', 42);
    pfa("x|42", "%1c|%d", 'x', 42);
    pfa("x|42", "%-c|%d", 'x', 42);
    pfa("[  x][y  ]", "[%3c][%-3c]", 'x', 'y');
    pfa("   %|42", "%4c|%d", '%', 42);
    /* %a and %A are not implemented yet: the conversion is printed as it
       stands, but its double is taken off the list */
    pfa("%a|42", "%a|%d", 1.0, 42);
    pfa("%A|42", "%A|%d", 1.0, 42);
    pfa("%.3a|42", "%.3a|%d", 1.0, 42);
    /* a conversion without a meaning, and no argument for it */
    pfa("%y|", "%y|");
    pfa("%5y|", "%5y|");
    /* a % at the very end: printed, and the NUL still ends the format */
    pfa("abc%", "abc%");
    pfa("abc%5", "abc%5");
    pfn();
}
