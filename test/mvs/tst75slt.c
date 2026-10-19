/*
 * tst75slt.c - MEASUREMENT PROBE for issue #162 (MVS target, batch).
 *
 * It measures a defect in the HERCULES X'75' TCP/IP emulation, NOT in
 * libc370.  find_slot() in Hercules' tcpip.c hands out an index into
 * map32[], the table of live conversation (talk) structures:
 *
 *     static u_int find_slot ( U_LONG_PTR address ) {
 *         u_int i = 0;
 *         obtain_lock (&tcpip_lock);
 *         while ((((signed) map32[i]) != -1) && (i < Ccom-1)) i++;
 *         map32[i] = address;             <-- assigns even when full
 *         release_lock (&tcpip_lock);
 *         return (i);
 *     }
 *
 * When every entry 0..Ccom-2 is in use the walk STOPS at Ccom-1 and assigns
 * there anyway.  map32[Ccom-1] is then overwritten on every further call, so
 * two or more talk structures alias one slot.  map32[R14] is what the second
 * X'75' instruction of a call dereferences to find the buffer pointers and
 * lengths of a transfer, so an aliased slot resolves to the WRONG session.
 *
 * ---------------------------------------------------------------------------
 * WARNING - READ BEFORE RUNNING
 * ---------------------------------------------------------------------------
 * This probe deliberately exhausts an EMULATOR-WIDE resource that is shared
 * by every address space on the system, and it can leave the X'75' table
 * corrupt and the emulator unstable or dead.  Run it ONLY in your own
 * disposable MVS/CE container, NEVER on a shared or real system, and NEVER
 * add it as a [[test]] entry - no routine `make test` must ever pick it up.
 * ---------------------------------------------------------------------------
 *
 * HOW THE LEAK IS FORCED
 * ----------------------
 * One guest recv() is two X'75' instructions.  The first (direction 0, "to
 * host") allocates the talk and its map32 slots; the second (direction 1,
 * "from host") copies the result back AND frees the talk and the slots.
 * Issue #162 names an abend between the two instructions as the way a slot
 * leaks in the field.  This probe reproduces the same host-side state more
 * directly and reproducibly: it issues ONLY the first X'75' instruction and
 * never issues the second (leak75() below).  The emulator has allocated the
 * talk and two map32 slots (the talk itself and its buffer_in) and has no
 * way to know the guest will never come back to free them.
 *
 * A recv on a freshly opened, never-connected socket is used: its host
 * select() times out at once, so EZASOKET's RECV path returns -2 BEFORE it
 * mallocs buffer_out - exactly two slots leak per call, no data moves, and
 * nothing blocks.  The first instruction stores R0-R15 back into the
 * parameter list, so R14 comes back holding the slot (the "Identifier")
 * find_slot() just assigned to the talk.
 *
 * WHAT IT MEASURES
 * ----------------
 *  1. The empirical slot limit: the highest DISTINCT identifier handed out
 *     before the numbers stop climbing.  Ccom is a compile-time constant in
 *     the emulator (1024 in this build), discovered here rather than assumed.
 *  2. The aliasing: once the table is full every further leak comes back with
 *     the SAME identifier (expected Ccom-1 == 1023).  A repeated identifier
 *     IS two talk structures aliasing one slot - the defect, seen whole from
 *     the guest, with no emulator instrumentation.  This is issue #162's own
 *     suggested evidence: "a socket handle that duplicates an existing one".
 *  3. A verification transfer AFTER exhaustion: an ordinary loopback recv
 *     whose data is checked end to end.  With every slot pinned at 1023 the
 *     emulator resolves the transfer's talk through an aliased map32 entry;
 *     the data comes back wrong, or the emulator faults.  THIS STEP MAY KILL
 *     THE EMULATOR, which is itself a valid #162 result, so every number
 *     above is emitted to the job log with wtof() BEFORE it runs.
 *
 * HOW TO READ THE RESULT
 * ----------------------
 * The job log (JESMSGLG) carries the wtof() lines; SYSPRINT carries the same
 * with more detail but is lost if the emulator dies mid-run.  Read, in order:
 *   TST75SLT limit=N pin=P aliasfrom=C  -> N distinct slots, pin value P,
 *                                          first duplicate at leak call C
 *   TST75SLT verify ...                 -> the post-exhaustion transfer
 * limit far below the leak-call count, and pin repeated on every later call,
 * is the find_slot() aliasing.  A verify line showing corrupt data, or the
 * absence of any verify line (emulator gone), is the consequence.
 *
 * BUILD (host; libc370 is the cc370 sysroot, not an mbt project):
 *     cc370 -O1 -Wall -Werror -Iinclude -I. -L build/sdk \
 *           test/mvs/tst75slt.c -o TST75SLT -flinker-output=iebcopy
 *     ld370 --pack TST75SLT=TST75SLT.iebcopy -o probe -xmit \
 *           --dsn IBMUSER.LIBC370.SLSCR
 * RUN: jcl/recv75slt.jcl then jcl/tst75slt.jcl, in your own container only.
 *
 * RC: 0 = the table was exhausted and aliasing (a duplicated identifier) was
 * observed; 8 = it was not reached or the socket setup failed.  RC says what
 * the GUEST saw; the emulator surviving the verify is reported separately.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mvs/wto.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <mvs/socket.h>
#include "src/net/dyn75/x75.h"

#define LEAKMAX     1200        /* hard cap on leak calls (2 slots each)    */
#define PINSEEN        6        /* consecutive repeats that confirm the pin */
#define PORTBASE   26200
#define PORTTRIES     10
#define WAITMAX      250        /* x 0.08s polls for the verify data        */
#define VBUFLEN      512        /* verification transfer length             */
#define PATTERN(i)  ((unsigned char)((i) % 251))

static unsigned char leakbuf[32];   /* recv dest for the leak calls (unused
                                       by the host: no data ever arrives)   */

static void snooze(void);
static unsigned leak75(int s);
static int vrecv(int s, void *buf, int len);

/*
 * leak75() - issue the FIRST X'75' instruction of a recv conversation and
 * stop.  The talk and its two map32 slots are allocated host-side and never
 * freed, because the second instruction (which frees them) is never issued.
 * Returns R14, the slot/identifier find_slot() assigned to the talk.
 * Mirrors @@75.c's __75() but omits the "from host" instruction entirely.
 */
static unsigned leak75(int s)
{
    PL75        pl;
    unsigned    regs[12];

    __asm__("XC\t0(64,%0),0(%0)     clear __75 parameter list"
        : : "r" (&pl) : "memory");

    pl.r6   = (unsigned) leakbuf;   /* dest buffer (unused, dir 0 only) */
    pl.r7   = (unsigned) 11;        /* function code for recv()         */
    pl.r8   = (unsigned) s;         /* socket number                    */
    pl.r9   = (unsigned) sizeof(leakbuf);

    /* dir 0 only: allocate the talk and its map32 slots, never free them.
       Keep each generated asm line short - a character in column 72 is a
       continuation indicator and eats the next statement. */
    __asm__(
    "LR\t14,%1            => save area\n\t"
    "STM\t0,11,0(14)      save R0-R11\n\t"
    "LR\t11,%0            => PL75\n\t"
    "LM\t0,9,0(11)        load R0-R9\n"
"*\n\t"
    "LA\t3,0              to host PC\n\t"
    "SLR\t0,0             restart = no\n\t"
    "DC\tX'75005000'      first X'75' only\n\t"
    "STM\t0,15,0(11)      R14 = slot\n\t"
    "LM\t0,11,0(14)       restore R0-R11"
    : : "r" (&pl), "r" (regs)
    : "0", "1", "3", "11", "14", "15", "memory");

    return pl.r14;
}

/* full recv() over a loopback pair, returns bytes read (via the library,
   both X'75' instructions) - used for the post-exhaustion verify */
static int vrecv(int s, void *buf, int len)
{
    int read = 0;
    int rc;

    while (read < len) {
        rc = recv(s, (char *)buf + read, len - read, 0);
        if (rc == -2) { snooze(); continue; }
        if (rc <= 0) break;
        read += rc;
    }
    return read;
}

static void snooze(void)
{
    __asm__("STIMER WAIT,BINTVL==F'8'   0.08 seconds" : : : "0", "1");
}

int main(void)
{
    int         s;
    unsigned    slot;
    unsigned    prev    = 0xFFFFFFFF;
    unsigned    maxslot = 0;
    unsigned    pin     = 0xFFFFFFFF;
    unsigned    aliasat = 0;
    int         repeats = 0;
    int         calls   = 0;
    int         pinned  = 0;
    int         rc      = 8;

    printf("TST75SLT - issue #162 probe: X'75' slot exhaustion / aliasing\n\n");
    printf("  OWN DISPOSABLE CONTAINER ONLY - exhausts an emulator-wide"
           " resource\n\n");

    /* one valid, never-connected socket to hang the leak recvs on */
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        printf("*** FAIL  socket() for the leak source\n");
        wtof("TST75SLT FAILED - no leak socket");
        return 8;
    }

    /* Leak two slots per call until the identifier stops climbing.  Each
       call issues only the first X'75' instruction, so the talk and its two
       map32 slots are allocated and never freed. */
    for (calls = 1; calls <= LEAKMAX; calls++) {
        slot = leak75(s);

        if (slot > maxslot) maxslot = slot;

        if (slot == prev) {
            if (repeats == 0) { pin = slot; aliasat = (unsigned)calls; }
            repeats++;
            if (repeats >= PINSEEN) { pinned = 1; break; }
        } else {
            repeats = 0;
        }
        prev = slot;

        if (calls <= 6 || (calls % 128) == 0) {
            printf("  leak %4d -> slot %u\n", calls, slot);
        }
    }

    printf("\n  leak calls issued : %d\n", calls);
    printf("  highest DISTINCT slot (empirical limit) : %u\n", maxslot);
    if (pinned) {
        printf("  pinned identifier : %u  (first duplicated at leak call %u)\n",
               pin, aliasat);
        printf("  => find_slot() handed the SAME slot to %d+ distinct talks:"
               " ALIASING\n", PINSEEN);
        rc = 0;
    } else {
        printf("  identifier never repeated within %d calls - pin NOT"
               " reached\n", LEAKMAX);
    }

    /* emit the verdict to the job log NOW, before the verify can crash the
       emulator and discard SYSPRINT */
    wtof("TST75SLT limit=%u pin=%d aliasfrom=%u calls=%d",
         maxslot, pinned ? (int)pin : -1, aliasat, calls);

    if (!pinned) {
        printf("\nTST75SLT FAILED - table not exhausted\n");
        wtof("TST75SLT FAILED - pin not reached");
        closesocket(s);
        return 8;
    }

    /* ----------------------------------------------------------------- */
    /* Verification transfer.  Every slot is pinned at %u now, so an      */
    /* ordinary recv resolves its talk through an aliased map32 entry.    */
    /* This may corrupt data or kill the emulator - hence the wtof above. */
    /* ----------------------------------------------------------------- */
    printf("\n  verification transfer (MAY CRASH THE EMULATOR)...\n");
    wtof("TST75SLT verify start - emulator may not survive this");

    {
        struct sockaddr_in  addr;
        static unsigned char out[VBUFLEN];
        static unsigned char in[VBUFLEN];
        int     lsock, csock = -1, ssock = -1, addrlen, i, got;
        unsigned port = 0;
        int     bad   = 0;

        for (i = 0; i < VBUFLEN; i++) out[i] = PATTERN(i);

        lsock = socket(AF_INET, SOCK_STREAM, 0);
        if (lsock < 0) { printf("    (no verify socket)\n"); goto doneverify; }

        for (i = 0; i < PORTTRIES; i++) {
            memset(&addr, 0, sizeof(addr));
            addr.sin_family      = AF_INET;
            addr.sin_port        = htons((unsigned short)(PORTBASE + i));
            addr.sin_addr.s_addr = 0x7F000001;
            if (bind(lsock, &addr, sizeof(addr)) >= 0) { port = PORTBASE + i; break; }
        }
        if (!port || listen(lsock, 5) < 0) {
            printf("    (no verify port)\n"); closesocket(lsock); goto doneverify;
        }

        csock = socket(AF_INET, SOCK_STREAM, 0);
        if (csock < 0 || connect(csock, &addr, sizeof(addr)) < 0) {
            printf("    (verify connect failed)\n");
            if (csock >= 0) closesocket(csock);
            closesocket(lsock); goto doneverify;
        }
        addrlen = sizeof(addr);
        ssock = accept(lsock, &addr, &addrlen);
        if (ssock < 0) {
            printf("    (verify accept failed)\n");
            closesocket(csock); closesocket(lsock); goto doneverify;
        }

        if (send(csock, out, VBUFLEN, 0) < 0) {
            printf("    (verify send failed)\n");
        } else {
            got = vrecv(ssock, in, VBUFLEN);
            for (i = 0; i < VBUFLEN && i < got; i++) {
                if (in[i] != PATTERN(i)) { bad = 1; break; }
            }
            printf("    verify: sent %d, got %d, %s\n",
                   VBUFLEN, got,
                   (got == VBUFLEN && !bad) ? "data INTACT" :
                   bad ? "data CORRUPT (aliasing confirmed guest-side)" :
                         "SHORT read");
            wtof("TST75SLT verify got=%d bad=%c first=%d",
                 got, bad ? 'Y' : 'N', bad ? i : -1);
        }
        closesocket(ssock); closesocket(csock); closesocket(lsock);
    }

doneverify:
    closesocket(s);
    printf("\nTST75SLT %s - aliasing observed; read the pin line above\n",
           rc ? "FAILED" : "PASSED");
    if (!rc) wtof("TST75SLT PASSED - aliasing observed");
    return rc;
}
