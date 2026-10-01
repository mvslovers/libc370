/*
 * tstinet.c - libc370 #51: <arpa/inet.h> on POSIX and BSD terms.
 *
 * 1.x had inet_aton() alone, with in_addr_t as a struct and the return
 * inverted -- 0 for an address, -1 for none -- so code written for BSD,
 * z/OS or Linux read every address as an error (brexx370's inet_addr() in
 * compat/jccompat.c does exactly that).  It also took "1.2.3.4x".  2.0 has
 * in_addr_t as an integer, inet_aton() returning 1/0, and inet_addr(),
 * inet_pton(), inet_ntop(), inet_ntoa().
 *
 * This compiles the REAL src/net/@@ina*.c / @@inp*.c / @@inn*.c and checks
 * known answers: the forms inet_aton() takes (one to four parts, octal,
 * hex), the ones it refuses (trailing junk, a fifth part, a part too large,
 * no digits, a sign, a leading blank, 2^32), inet_pton()'s stricter form,
 * inet_ntop()'s ENOSPC boundary, and inet_ntoa()'s buffer from __wsaget().
 *
 * Addresses are compared as values built with shifts, which is network
 * byte order on the target (big-endian) and consistent with the code on
 * the host, which builds them the same way.
 *
 * BUILD / RUN: test/host/run.sh tstinet
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "../../include/arpa/inet.h"

/* libc370's <string.h> carries S/370 inline assembler a host compiler cannot
** take, so the three string helpers the test needs are its own */
static int t_strcmp(const char *a, const char *b)
{ while (*a && *a == *b) a++, b++; return (unsigned char) *a - (unsigned char) *b; }
static void t_fill(void *d, int c, size_t n) { char *p = d; while (n--) *p++ = (char) c; }
static void t_copy(void *d, const void *s, size_t n)
{ char *p = d; const char *q = s; while (n--) *p++ = *q++; }

/* errno, as <errno.h> reaches it */
static int the_errno;
int *__errno(void) { return &the_errno; }

/* __wsaget(): one area per key, a copy of the template, as on MVS */
static int  wsa_fail;
static void *wsa_key;
static void *wsa_area;
void *__wsaget(void *key, unsigned len)
{
    if (wsa_fail) return NULL;
    if (wsa_key != key) {
        wsa_key  = key;
        wsa_area = calloc(1, len);
        t_copy(wsa_area, key, len);
    }
    return wsa_area;
}

static int fails, checks;
#define CHECK(cond, msg)                                                    \
    do { checks++; if (!(cond)) { fails++;                                  \
         printf("  FAIL %s\n", msg); } } while (0)
#define CHECK_EQ(got, want, msg)                                            \
    do { unsigned long g_ = (unsigned long) (got), w_ = (unsigned long) (want); \
         checks++; if (g_ != w_) { fails++;                                 \
         printf("  FAIL %s: got 0x%lX, want 0x%lX\n", msg, g_, w_); } } while (0)
#define CHECK_STR(got, want, msg)                                           \
    do { const char *g_ = (got); checks++;                                  \
         if (!g_ || t_strcmp(g_, want)) { fails++;                            \
         printf("  FAIL %s: got \"%s\", want \"%s\"\n", msg,                \
                g_ ? g_ : "(null)", want); } } while (0)

static void aton_ok(const char *cp, unsigned long want)
{
    struct in_addr a = { 0xDEADBEEFUL };
    char msg[80];
    snprintf(msg, sizeof msg, "inet_aton(\"%s\") is an address", cp);
    CHECK_EQ(inet_aton(cp, &a), 1, msg);
    snprintf(msg, sizeof msg, "inet_aton(\"%s\") value", cp);
    CHECK_EQ(a.s_addr, want, msg);
}

static void aton_bad(const char *cp)
{
    struct in_addr a = { 0xDEADBEEFUL };
    char msg[80];
    snprintf(msg, sizeof msg, "inet_aton(\"%s\") refused", cp ? cp : "NULL");
    CHECK_EQ(inet_aton(cp, &a), 0, msg);
    snprintf(msg, sizeof msg, "inet_aton(\"%s\") leaves *inp", cp ? cp : "NULL");
    CHECK_EQ(a.s_addr, 0xDEADBEEFUL, msg);
}

static void pton(const char *src, int want, unsigned long val)
{
    struct in_addr a = { 0xDEADBEEFUL };
    char msg[80];
    snprintf(msg, sizeof msg, "inet_pton(\"%s\")", src);
    CHECK_EQ(inet_pton(AF_INET, src, &a), want, msg);
    snprintf(msg, sizeof msg, "inet_pton(\"%s\") value", src);
    CHECK_EQ(a.s_addr, want == 1 ? val : 0xDEADBEEFUL, msg);
}

int main(void)
{
    struct in_addr  a;
    char            buf[INET_ADDRSTRLEN + 4];
    char            *p, *q;

    /* 1. inet_aton(): the forms it takes */
    aton_ok("127.0.0.1",        0x7F000001UL);
    aton_ok("0.0.0.0",          0x00000000UL);
    aton_ok("255.255.255.255",  0xFFFFFFFFUL);
    aton_ok("10.1.2",           0x0A010002UL);    /* c: 16 bits */
    aton_ok("10.65535",         0x0A00FFFFUL);    /* b: 24 bits */
    aton_ok("10.1",             0x0A000001UL);
    aton_ok("167772161",        0x0A000001UL);    /* a: 32 bits */
    aton_ok("4294967295",       0xFFFFFFFFUL);
    aton_ok("0x7f.1",           0x7F000001UL);    /* hex */
    aton_ok("0X7F.0.0.0x1",     0x7F000001UL);
    aton_ok("010.0.0.1",        0x08000001UL);    /* octal */
    aton_ok("1.2.3.4 trailer",  0x01020304UL);    /* ends at a blank */
    CHECK_EQ(inet_aton("1.2.3.4", NULL), 1, "inet_aton() with inp NULL checks only");

    /* 2. inet_aton(): what it refuses; 1.x took the first */
    aton_bad("1.2.3.4x");
    aton_bad("1.2.3.4.5");
    aton_bad("256.1.1.1");
    aton_bad("1.2.3.256");
    aton_bad("1.256.65536");
    aton_bad("1.16777216");
    aton_bad("4294967296");
    aton_bad("08.1.1.1");                       /* 8 is no octal digit */
    aton_bad("0x");
    aton_bad("0x.1.1.1");
    aton_bad("");
    aton_bad("1..2");
    aton_bad("1.2.3.");
    aton_bad(".1.2.3");
    aton_bad("-1");
    aton_bad("+1.2.3.4");
    aton_bad(" 1.2.3.4");
    aton_bad(NULL);

    /* 3. inet_addr() */
    CHECK_EQ(inet_addr("192.168.1.1"), 0xC0A80101UL, "inet_addr(\"192.168.1.1\")");
    CHECK_EQ(inet_addr("10.1"),        0x0A000001UL, "inet_addr(\"10.1\")");
    CHECK_EQ(inet_addr("x"),           INADDR_NONE,  "inet_addr(\"x\") is INADDR_NONE");
    CHECK_EQ(inet_addr(NULL),          INADDR_NONE,  "inet_addr(NULL) is INADDR_NONE");

    /* 4. inet_pton(): four decimal parts, nothing else */
    pton("1.2.3.4",          1, 0x01020304UL);
    pton("0.0.0.0",          1, 0x00000000UL);
    pton("255.255.255.255",  1, 0xFFFFFFFFUL);
    pton("10.0.0.10",        1, 0x0A00000AUL);
    pton("01.2.3.4",         0, 0);              /* leading zero */
    pton("1.2.3",            0, 0);
    pton("1.2.3.4.5",        0, 0);
    pton("256.0.0.0",        0, 0);
    pton("1.2.3.4 ",         0, 0);
    pton("0x1.2.3.4",        0, 0);
    pton("",                 0, 0);
    the_errno = 0;
    CHECK_EQ(inet_pton(AF_INET6, "::1", &a), (unsigned long) -1, "inet_pton(AF_INET6) is -1");
    CHECK_EQ(the_errno, EAFNOSUPPORT, "inet_pton(AF_INET6) errno");

    /* 5. inet_ntop() */
    a.s_addr = 0x7F000001UL;
    CHECK_STR(inet_ntop(AF_INET, &a, buf, sizeof buf), "127.0.0.1", "inet_ntop(127.0.0.1)");
    a.s_addr = 0;
    CHECK_STR(inet_ntop(AF_INET, &a, buf, sizeof buf), "0.0.0.0", "inet_ntop(0.0.0.0)");
    a.s_addr = 0xC0A8640AUL;
    CHECK_STR(inet_ntop(AF_INET, &a, buf, sizeof buf), "192.168.100.10", "inet_ntop(192.168.100.10)");
    a.s_addr = 0xFFFFFFFFUL;
    CHECK_STR(inet_ntop(AF_INET, &a, buf, INET_ADDRSTRLEN), "255.255.255.255",
              "inet_ntop() fits INET_ADDRSTRLEN exactly");
    the_errno = 0;
    t_fill(buf, 'Z', sizeof buf);
    CHECK(inet_ntop(AF_INET, &a, buf, INET_ADDRSTRLEN - 1) == NULL, "inet_ntop() one byte short is NULL");
    CHECK_EQ(the_errno, ENOSPC, "inet_ntop() one byte short: errno");
    CHECK(buf[0] == 'Z', "inet_ntop() one byte short leaves dst");
    the_errno = 0;
    CHECK(inet_ntop(AF_INET6, &a, buf, sizeof buf) == NULL, "inet_ntop(AF_INET6) is NULL");
    CHECK_EQ(the_errno, EAFNOSUPPORT, "inet_ntop(AF_INET6) errno");

    /* 6. inet_ntoa(): a buffer per process, overwritten by the next call */
    a.s_addr = 0x0A000001UL;
    p = inet_ntoa(a);
    CHECK_STR(p, "10.0.0.1", "inet_ntoa(10.0.0.1)");
    a.s_addr = 0x0A000002UL;
    q = inet_ntoa(a);
    CHECK(p == q, "inet_ntoa() reuses its buffer");
    CHECK_STR(p, "10.0.0.2", "inet_ntoa() second call overwrites the first");
    wsa_fail = 1;
    CHECK(inet_ntoa(a) == NULL, "inet_ntoa() without a process anchor is NULL");
    wsa_fail = 0;

    /* 7. round trip */
    {
        static const char *rt[] = { "1.2.3.4", "10.0.0.255", "172.16.254.1", "255.0.255.0" };
        unsigned i;
        for (i = 0; i < sizeof rt / sizeof rt[0]; i++) {
            CHECK_EQ(inet_pton(AF_INET, rt[i], &a), 1, rt[i]);
            CHECK_STR(inet_ntop(AF_INET, &a, buf, sizeof buf), rt[i], rt[i]);
        }
    }

    printf("tstinet: %d of %d checks passed\n", checks - fails, checks);
    return fails ? 1 : 0;
}
