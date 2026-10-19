/*
 * tst75bnd.c - MEASUREMENT PROBE for issue #164 (MVS target, batch).
 *
 * It measures a defect in the HERCULES X'75' TCP/IP emulation, NOT in
 * libc370.  The SELECT path in Hercules' tcpip.c takes aux2 straight from a
 * guest register and uses it to index Ccom_han[] (size Ccom == 1024) WITHOUT
 * any bounds check, e.g. subcode 5 "Read Outputs":
 *
 *     i = 0;
 *     while (i < aux2) {
 *         if (Ccom_han [i] != -1)                 <-- OOB read for i >= Ccom
 *             if (FD_ISSET (Ccom_han [i], ...))
 *                 hercFD_SET (i, (hercfd_set *)(t->buffer_out));  <-- OOB write
 *         i++;
 *     }
 *
 * and subcode 4 "Run" does select (Ccom_han [aux2 - 1] + 1, ...).  aux2 is
 * the guest's maxsock; libc370 never passes an out-of-range one, so this path
 * has never been exercised.  A large aux2 walks Ccom_han[] far off its end
 * (a guest-controlled out-of-bounds READ) and, where a bit happens to pass
 * the FD_ISSET test, writes the result bitmap past its allocation (a
 * guest-controlled out-of-bounds WRITE) - all in the emulator's own address
 * space, which is shared by every guest.
 *
 * ---------------------------------------------------------------------------
 * WARNING - READ BEFORE RUNNING
 * ---------------------------------------------------------------------------
 * This probe can CRASH THE EMULATOR, and the out-of-bounds WRITE can corrupt
 * state belonging to OTHER address spaces before it does.  Run it ONLY in
 * your own disposable MVS/CE container, give it its own instance, and expect
 * to restart it.  NEVER run it on a shared or real system, and NEVER add it
 * as a [[test]] entry - a routine `make test` that picked it up would be a
 * trap that takes the emulator down.
 * ---------------------------------------------------------------------------
 *
 * HOW IT DRIVES THE PATH
 * ----------------------
 * No libc370 wrapper will pass a bad aux2, so the parameter list is built by
 * hand and issued through the low-level __75() entry.  One SELECT run is a
 * sequence of subcodes keyed on a socket number (aux1 is the subcode, aux2
 * the maxsock, func = (socket<<16)|17):
 *   - subcode 0 "Start"  allocates the per-socket select state (required
 *     before any other subcode, which otherwise return -1).
 *   - subcode 5 "Read Outputs" with a BAD aux2 is the measured call: it walks
 *     Ccom_han[0..aux2-1] unconditionally.
 *   - subcode 8 "Finish" frees the select state (best-effort cleanup).
 *
 * The aux2 values are escalated in documented steps, from just past the
 * legitimate maximum up to a stated ceiling.  After each value the probe
 * confirms the emulator is still alive by opening and closing an ordinary
 * socket; if that stops working, or the job never returns, the emulator did
 * not survive that value.  Every value and its outcome is written to the job
 * log with wtof() BEFORE the next, riskier value, so the record survives a
 * crash that discards SYSPRINT.
 *
 * HOW TO READ THE RESULT
 * ----------------------
 * In the job log (JESMSGLG), one line per value tried:
 *   TST75BND aux2=V rc=R alive=Y/N   -> value V returned R; an ordinary
 *                                       socket still worked afterwards (Y/N)
 * followed by:
 *   TST75BND highest aux2 tried = V
 * The last aux2 with a line, with NO "alive=N" and NO missing tail, is the
 * highest survivable value measured on this build.  An alive=N line, or the
 * run ending with no "highest" line and the container no longer answering, is
 * the emulator going down at that value.  A negative result means only that
 * the values tried were survivable - the stated ceiling is the bound of what
 * was measured, not a proof the path is safe.
 *
 * BUILD (host; libc370 is the cc370 sysroot, not an mbt project):
 *     cc370 -O1 -Wall -Werror -Iinclude -I. -L build/sdk \
 *           test/mvs/tst75bnd.c -o TST75BND -flinker-output=iebcopy
 *     ld370 --pack TST75BND=TST75BND.iebcopy -o probe -xmit \
 *           --dsn IBMUSER.LIBC370.BNSCR
 * RUN: jcl/recv75bnd.jcl then jcl/tst75bnd.jcl, in your own container only.
 *
 * RC: 0 = every value tried was survived by the emulator and by the guest;
 * 8 = the socket setup failed.  A crash has no RC - it is read from the
 * ABSENCE of the tail lines plus the container no longer answering.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mvs/wto.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <mvs/socket.h>
#include "src/net/dyn75/x75.h"

#define NVAL 7

/* SELECT via the raw __75() entry.  func = (socket<<16)|17, aux1 = subcode,
   aux2 = maxsock.  Returns the emulator's ret_cd (R4). */
static int sel(int sock, unsigned subcode, unsigned aux2, void *buf, int len)
{
    PL75    pl;

    __asm__("XC\t0(64,%0),0(%0)     clear __75 parameter list"
        : : "r" (&pl) : "memory");

    pl.r1 = (unsigned) len;                 /* len_in (bytes copied in)   */
    pl.r5 = (unsigned) buf;                 /* source buffer for set-ins  */
    pl.r7 = (unsigned) (((unsigned)sock << 16) | 17);   /* func: SELECT   */
    pl.r8 = (unsigned) subcode;             /* aux1: SELECT subcode       */
    pl.r9 = (unsigned) aux2;                /* aux2: maxsock (UNCHECKED)  */

    __75(&pl);
    return (int) pl.r4;
}

/* open then close an ordinary socket; returns 1 if the emulator still serves
   the X'75' path normally, 0 otherwise */
static int emulator_alive(void)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return 0;
    closesocket(s);
    return 1;
}

int main(void)
{
    /* the escalation ladder: just past the legitimate max, then up to a
       stated ceiling.  Ccom is 1024 in this build. */
    static const unsigned val[NVAL] = {
        8,              /* just past a handful of real sockets      */
        1025,           /* one past Ccom                            */
        1032,           /* a few ints past the array                */
        2048,           /* one page of ints past it                 */
        65536,          /* 256 KB past the array                    */
        0x00040000,     /* 1 MB  past: likely still mapped          */
        0x01000000      /* 64 MB past the array: the ceiling tried  */
    };
    int         sock;
    unsigned    i;
    int         rc;
    int         alive;

    printf("TST75BND - issue #164 probe: unchecked aux2 in X'75' SELECT\n\n");
    printf("  OWN DISPOSABLE CONTAINER ONLY - this can crash the emulator\n\n");

    /* establish the legitimate socket-number range first */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("*** FAIL  socket() for the SELECT target\n");
        wtof("TST75BND FAILED - no socket");
        return 8;
    }
    printf("  legitimate socket number handed out: %d"
           " (so legal maxsock ~= %d)\n\n", sock, sock + 1);
    wtof("TST75BND legal socket=%d", sock);

    for (i = 0; i < NVAL; i++) {
        /* Start: allocate the per-socket select state (subcodes 1..7 need
           it, else they return -1 before touching aux2) */
        (void) sel(sock, 0, (unsigned)(sock + 1), 0, 0);

        printf("  aux2 = %-10u (0x%08X) ... ", val[i], val[i]);

        /* subcode 5 "Read Outputs" walks Ccom_han[0..aux2-1] with no check */
        rc = sel(sock, 5, val[i], 0, 0);

        /* Finish: free the select state (best effort; skipped if we died) */
        (void) sel(sock, 8, (unsigned)(sock + 1), 0, 0);

        alive = emulator_alive();

        printf("rc=%d  emulator %s\n", rc, alive ? "ALIVE" : "DEAD");
        wtof("TST75BND aux2=%u rc=%d alive=%c",
             val[i], rc, alive ? 'Y' : 'N');

        if (!alive) {
            printf("\n  emulator stopped serving sockets at aux2=%u\n", val[i]);
            wtof("TST75BND highest aux2 tried = %u (emulator DEAD)", val[i]);
            closesocket(sock);
            return 8;
        }
    }

    printf("\n  highest aux2 tried = %u (0x%08X) - all survived\n",
           val[NVAL - 1], val[NVAL - 1]);
    printf("  NB: survival of these values is NOT proof the path is safe.\n");
    wtof("TST75BND highest aux2 tried = %u - all survived", val[NVAL - 1]);

    closesocket(sock);
    printf("\nTST75BND PASSED - every value survived; see the job log\n");
    return 0;
}
