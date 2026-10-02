/*
 * tstscncase.h - the cases of test/host/tstscnll.c and
 * test/mvs/tstscnll.c (libc370 #318): scanf's length modifiers.
 *
 * The includer supplies CHECK(cond, msg) and
 * int scan(const char *s, const char *f, ...) -- the host's drives
 * vvscanf() directly, the MVS one is sscanf() itself.  Every target is a
 * member of a struct between two guard bytes, so a store of the wrong
 * width changes a guard.
 */

#define GUARD 0x5A

#define BOXED(T) struct { unsigned char lo; T v; unsigned char hi; }
#define BOX(b) do { memset(&(b), GUARD, sizeof(b)); } while (0)
#define INTACT(b) ((b).lo == GUARD && (b).hi == GUARD)

static void scn_cases(void)
{
    BOXED(signed char) c;
    BOXED(unsigned char) uc;
    BOXED(short) h;
    BOXED(long) l;
    BOXED(unsigned long) ul;
    BOXED(long long) ll;
    BOXED(unsigned long long) ull;
    BOXED(int) n;
    BOXED(double) d;
    BOXED(long double) e;
    BOXED(float) f;
    int r;

    BOX(c);
    r = scan("-12", "%hhd", &c.v);
    CHECK(r == 1 && c.v == -12 && INTACT(c), "%hhd -12, one byte");
    BOX(uc);
    r = scan("ff", "%hhx", &uc.v);
    CHECK(r == 1 && uc.v == 0xFF && INTACT(uc), "%hhx ff");
    BOX(h);
    r = scan("-1234", "%hd", &h.v);
    CHECK(r == 1 && h.v == -1234 && INTACT(h), "%hd still works");
    BOX(l);
    r = scan("-123456", "%ld", &l.v);
    CHECK(r == 1 && l.v == -123456 && INTACT(l), "%ld still works");

    BOX(ll);
    r = scan("-5", "%lld", &ll.v);
    CHECK(r == 1 && ll.v == -5 && INTACT(ll), "%lld -5");
    BOX(ll);
    r = scan("123456789012", "%lld", &ll.v);
    CHECK(r == 1 && ll.v == 123456789012LL && INTACT(ll),
          "%lld 123456789012, past 32 bits");
    BOX(ll);
    r = scan("-9223372036854775808", "%lld", &ll.v);
    CHECK(r == 1 && ll.v == (-9223372036854775807LL - 1) && INTACT(ll),
          "%lld LLONG_MIN");
    BOX(ll);
    r = scan("9223372036854775807", "%lli", &ll.v);
    CHECK(r == 1 && ll.v == 9223372036854775807LL && INTACT(ll),
          "%lli LLONG_MAX");
    BOX(ull);
    r = scan("ffffffffffffffff", "%llx", &ull.v);
    CHECK(r == 1 && ull.v == 0xFFFFFFFFFFFFFFFFULL && INTACT(ull),
          "%llx 16 x f");
    BOX(ull);
    r = scan("18446744073709551615", "%llu", &ull.v);
    CHECK(r == 1 && ull.v == 0xFFFFFFFFFFFFFFFFULL && INTACT(ull),
          "%llu ULLONG_MAX");
    BOX(ll);
    r = scan("-77", "%jd", &ll.v);
    CHECK(r == 1 && ll.v == -77 && INTACT(ll), "%jd (intmax_t)");
    BOX(ll);
    r = scan("4294967296", "%Ld", &ll.v);
    CHECK(r == 1 && ll.v == 4294967296LL && INTACT(ll), "%Ld as long long");

    BOX(ul);
    r = scan("4000000000", "%zu", &ul.v);
    CHECK(r == 1 && ul.v == 4000000000UL && INTACT(ul), "%zu (size_t)");
    BOX(l);
    r = scan("-42", "%td", &l.v);
    CHECK(r == 1 && l.v == -42 && INTACT(l), "%td (ptrdiff_t)");

    BOX(ul);
    r = scan("-5", "%lu", &ul.v);
    CHECK(r == 1 && ul.v == (unsigned long)-5L && INTACT(ul),
          "%lu -5 is the negation, as strtoul");

    BOX(ll);
    BOX(l);
    r = scan("11 22", "%lld %ld", &ll.v, &l.v);
    CHECK(r == 2 && ll.v == 11 && l.v == 22 && INTACT(ll) && INTACT(l),
          "%lld then %ld: the next argument is the right one");
    BOX(ll);
    BOX(h);
    r = scan("7 8", "%jd %hd", &ll.v, &h.v);
    CHECK(r == 2 && ll.v == 7 && h.v == 8, "%jd then %hd");

    BOX(c);
    BOX(ll);
    BOX(n);
    r = scan("abc", "abc%hhn", &c.v);
    CHECK(c.v == 3 && INTACT(c), "%hhn");
    r = scan("abcd", "abcd%lln", &ll.v);
    CHECK(ll.v == 4 && INTACT(ll), "%lln");
    r = scan("ab", "ab%n", &n.v);
    CHECK(n.v == 2 && INTACT(n), "%n still an int");

    /* vvscanf stores a double for L: right where long double is double
       (cc370, and the arm64 macOS host); an x86 host would differ */
    BOX(e);
    r = scan("1.5", "%Lf", &e.v);
    CHECK(r == 1 && e.v == 1.5 && INTACT(e), "%Lf into a long double");
    BOX(d);
    r = scan("2.5", "%lf", &d.v);
    CHECK(r == 1 && d.v == 2.5 && INTACT(d), "%lf still double");
    BOX(f);
    r = scan("0.5", "%f", &f.v);
    CHECK(r == 1 && f.v == 0.5f && INTACT(f), "%f still float");
}
