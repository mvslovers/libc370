/*
 * tst75dns.c - libc370 #163 probe (MVS target, batch): does a name lookup
 * through X'75' stop the whole emulated CPU while the host resolver works?
 *
 * Not a libc370 defect.  It measures the Hercules X'75' emulation: tcpip.c
 * calls the host's gethostbyname() (function 4) and gethostbyaddr()
 * (function 18) inline, inside the instruction, on the CPU thread.  If that
 * blocks, nothing else in the guest runs until the host resolver answers -
 * the class of the SEND defect fixed upstream as hyperion #863 / PR #864,
 * where the watchdog (WATCHDOG_SECS 20) killed the emulator.
 *
 * HOW IT MEASURES
 * ---------------
 * The lookup and the observer are different tasks.  An observer subtask
 * (cthread) loops: STCK, note the gap since its previous STCK, STIMER WAIT
 * 0.01 s.  It never spins, so on one CPU it cannot starve the lookup task or
 * be starved by it; its gap is the dispatching noise, 10-20 ms.  The main
 * task resets the observer's maximum, does one lookup timed with STCK
 * before and after, waits 0.3 s for the observer to run again, and reads
 * the observer's largest gap.
 *
 *     gap ~ lookup duration  -> the CPU was stopped for the lookup
 *     gap ~ noise floor      -> the observer kept running; not blocked
 *
 * Cases: idle (no lookup) for the noise floor; "localhost" and 127.0.0.1 as
 * fast controls (the addr control must come back with a name, or the
 * gethostbyaddr path is not working at all and its slow cases say nothing);
 * then two slow forward and two slow reverse lookups; idle again.
 *
 * THE SLOW NAMES: a resolver has to wait for servers that never answer.  The
 * reverse zones under 6.in-addr.arpa are delegated to servers that do not
 * respond, so a recursive resolver gives up after seconds (SERVFAIL).
 * Measured 2026-10-19 with getent inside the mvsce image on this Mac:
 * 3-8 s per lookup.  A repeated name is cached and fast, so every slow case
 * uses a fresh name built from the TOD clock: forward h<n>.<a>.<b>.6.in-
 * addr.arpa, reverse 6.<a>.<b>.<c>.  A slow case that came back fast
 * (< 1 s) is reported and does not count towards a verdict.
 *
 * CAVEATS
 * -------
 * - The lookup time belongs to the resolver of the machine Hercules runs
 *   on (here a local container: Debian, nameserver 192.168.64.1, the Mac's
 *   resolver behind it).  A stand has another resolver, another cache and
 *   other timeouts.  What carries over is whether the CPU blocks, not how
 *   long.
 * - With NUMCPU > 1 MVS can dispatch the observer on another CPU, and a
 *   stall of one CPU thread then does not show as an observer gap.  The
 *   mvsce container runs NUMCPU 1.
 * - A lookup that blocks for 20 s or more can trip the watchdog.  glibc's
 *   worst case is about 10 s (timeout 5 s x 2 attempts).
 *
 * BUILD (host, from the repository root, after make):
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tst75dns.c \
 *           -o TST75DNS -flinker-output=iebcopy
 *     ld370 --pack TST75DNS=TST75DNS.iebcopy -o out -xmit \
 *           --dsn IBMUSER.LIBC370.DNSCR
 *
 * RUN: jcl/recv75dns.jcl (RECEIVE from IBMUSER.LIBC370.DNXMIT), then
 * jcl/tst75dns.jcl.  Every result line is a WTO in the job log:
 *     TST75DNS <case> <name> -> <result> dur=<ms> gap=<ms> ticks=<n>
 *     TST75DNS RESULT ...
 * dur is the lookup as the lookup task timed it, gap the observer's largest
 * gap around it, ticks how often the observer ran in that window.
 *
 * RC: 0 = slow lookups ran (>= 1 s) and the observer kept running: no block;
 *     4 = the observer stalled for most of a slow lookup: the CPU blocks;
 *     8 = no slow lookup could be produced, or setup failed: no verdict.
 */
#include <stdio.h>
#include <string.h>
#include <mvs/wto.h>
#include <mvs/thread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <mvs/socket.h>
#include <netdb.h>

#define SLOW_MS     1000            /* a slow case must take at least this */

static volatile int      obs_stop;
static volatile int      obs_reset;
static volatile unsigned obs_max;   /* largest gap, microseconds          */
static volatile unsigned obs_ticks;

/* the TOD clock in microseconds, modulo 2**32 (71 minutes): deltas only */
static unsigned now_us(void)
{
    unsigned tod[2];

    __asm__("LA\t2,%0\tget address of 8 byte work area\n\t"
            "STCK\t0(2)\tstore clock into work area"
            : "=m" (tod) : : "2", "memory");

    return (tod[0] << 20) | (tod[1] >> 12);
}

static void snooze(unsigned hundredths)
{
    unsigned t = hundredths;

    __asm__("STIMER WAIT,BINTVL=(%0)" : : "r" (&t) : "0", "1", "14", "15");
}

static int observer(void *a, void *b)
{
    unsigned prev, cur, gap;

    (void)a;
    (void)b;
    prev = now_us();
    while (!obs_stop) {
        snooze(1);
        cur = now_us();
        if (obs_reset) {
            obs_max   = 0;
            obs_ticks = 0;
            obs_reset = 0;
        } else {
            gap = cur - prev;
            if (gap > obs_max) obs_max = gap;
            obs_ticks++;
        }
        prev = cur;
    }
    return 0;
}

static int nslow, nblock, nfree;

/*
 * One case.  kind: 0 idle, 1 gethostbyname(name), 2 gethostbyaddr(addr).
 * slow: whether this case is meant to be slow and counts for the verdict.
 */
static void run_case(const char *tag, int kind, const char *name,
                     unsigned addr, int slow, int *ok)
{
    char            res[48];
    struct hostent  *h = NULL;
    unsigned        t0, t1, dur, gap, ticks;
    unsigned        a = addr;

    obs_reset = 1;
    snooze(20);                     /* the observer applies the reset    */

    t0 = now_us();
    if (kind == 1) {
        h = gethostbyname(name);
    } else if (kind == 2) {
        h = gethostbyaddr(&a);
    } else {
        snooze(300);
    }
    t1 = now_us();

    snooze(30);                     /* the observer runs again           */
    gap   = obs_max;
    ticks = obs_ticks;
    dur   = t1 - t0;

    if (kind == 0) {
        strcpy(res, "-");
    } else if (!h) {
        strcpy(res, "NULL");
    } else if (kind == 1) {
        unsigned ip = *(unsigned *)h->h_addr_list[0];
        sprintf(res, "%u.%u.%u.%u", ip >> 24, (ip >> 16) & 255,
                (ip >> 8) & 255, ip & 255);
    } else {
        strncpy(res, h->h_name, sizeof(res) - 1);
        res[sizeof(res) - 1] = 0;
    }
    if (ok) *ok = (h != NULL);

    wtof("TST75DNS %s %s -> %s dur=%u gap=%u ticks=%u", tag, name, res,
         dur / 1000, gap / 1000, ticks);

    if (!slow) return;
    if (dur / 1000 < SLOW_MS) {
        wtof("TST75DNS %s was fast (%u ms) - not counted", tag, dur / 1000);
        return;
    }
    nslow++;
    if (gap >= dur / 2) nblock++;
    else                nfree++;
}

int main(void)
{
    CTHDTASK *task;
    char     fwd[2][48], rev[2][24];
    unsigned radr[2];
    unsigned x;
    int      i;
    int      ctl_name = 0, ctl_addr = 0;

    printf("TST75DNS - libc370 #163 probe, see the job log\n");

    task = cthread_create((void *)observer, NULL, NULL);
    if (!task) {
        wtof("TST75DNS RESULT setup: cthread_create failed - RC 8");
        return 8;
    }
    snooze(50);

    /* fresh names: a repeated one is cached and measures nothing */
    for (i = 0; i < 2; i++) {
        x = now_us() + (unsigned)i * 7919u;
        sprintf(fwd[i], "h%u.%u.%u.6.in-addr.arpa", x % 100000u,
                (x >> 8) & 255, (x >> 16) & 255);
        radr[i] = (6u << 24) | (((x >> 4) & 255) << 16)
                | (((x >> 12) & 255) << 8) | ((x >> 20) & 255);
        sprintf(rev[i], "6.%u.%u.%u", (radr[i] >> 16) & 255,
                (radr[i] >> 8) & 255, radr[i] & 255);
    }

    run_case("idle1", 0, "-", 0, 0, NULL);
    run_case("ctl-name", 1, "localhost", 0, 0, &ctl_name);
    run_case("ctl-addr", 2, "127.0.0.1", 0x7F000001, 0, &ctl_addr);
    run_case("slow-name1", 1, fwd[0], 0, 1, NULL);
    run_case("slow-addr1", 2, rev[0], radr[0], ctl_addr, NULL);
    run_case("slow-name2", 1, fwd[1], 0, 1, NULL);
    run_case("slow-addr2", 2, rev[1], radr[1], ctl_addr, NULL);
    run_case("idle2", 0, "-", 0, 0, NULL);

    obs_stop = 1;
    cthread_wait(&task->termecb);
    cthread_delete(&task);

    if (!ctl_addr)
        wtof("TST75DNS gethostbyaddr control failed: addr cases not counted");

    if (nblock) {
        wtof("TST75DNS RESULT BLOCKS: %d of %d slow lookups stalled the "
             "observer - RC 4", nblock, nslow);
        return 4;
    }
    if (!nslow) {
        wtof("TST75DNS RESULT NO VERDICT: no slow lookup (ctl %d/%d) - RC 8",
             ctl_name, ctl_addr);
        return 8;
    }
    wtof("TST75DNS RESULT NO BLOCK: %d slow lookups, observer kept "
         "running - RC 0", nfree);
    return 0;
}
