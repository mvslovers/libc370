/*
 * tstcrt.c - libc370 #159 on MVS: one CRT, IDENTIFY CTHREAD only when the
 * program uses threads.
 *
 * ISSUE #159: @@crt0 and @@crt1 differed in one IDENTIFY.  crt0
 * IDENTIFYed CTHREAD in every program, crt1 in none, and the ecosystem
 * linked crt1 and IDENTIFYed by hand.  Now there is one @@CRT0, a member
 * of libc.a (crt0.o and crt1.o are copies of it), and CTHREAD/@@CTEXIT
 * are a member of their own: cthread_create_ex() references CTHREAD hard,
 * @@CRT0 weakly, so the startup IDENTIFYs CTHREAD exactly when the
 * program can create a thread.
 *
 * Built twice from this source: without WITH_THREADS (the CDE for CTHREAD
 * must NOT exist when main() starts) and with it (the CDE must exist
 * before any thread call, and a thread must run without the program
 * IDENTIFYing anything).  Each is linked three ways:
 *
 *   TSTCRTN / TSTCRTT    no startfile: @@CRT0 comes out of libc.a
 *   TSTCRTN0 / TSTCRTT0  this tree's crt0.o first, as the cc370 driver does
 *   TSTCRTNR / TSTCRTTR  red control: the installed sysroot's crt0.o
 *                        (IDENTIFYs always) and crt1.o (never), before #159
 *
 * Build (R = this tree, L = the installed sysroot's lib):
 *   cc370 -O1 -Iinclude -c test/mvs/tstcrt.c -o n.o
 *   cc370 -O1 -Iinclude -DWITH_THREADS -c test/mvs/tstcrt.c -o t.o
 *   ld370 --entry @@CRT0 n.o -L build/sdk -lc -L $L -lcc370rt \
 *         -iebcopy -o TSTCRTN
 *   ld370 --entry @@CRT0 t.o -L build/sdk -lc -L $L -lcc370rt \
 *         -iebcopy -o TSTCRTT
 *   ld370 --entry @@CRT0 build/sdk/crt0.o n.o -L build/sdk -lc \
 *         -L $L -lcc370rt -iebcopy -o TSTCRTN0           (TSTCRTT0 alike)
 *   ld370 --entry @@CRT0 $L/crt0.o n.o -L $L -lc -lcc370rt \
 *         -iebcopy -o TSTCRTNR
 *   ld370 --entry @@CRT0 $L/crt1.o t.o -L $L -lc -lcc370rt \
 *         -iebcopy -o TSTCRTTR
 *   ld370 --pack TSTCRTN=TSTCRTN.iebcopy ... -o tstcrt -xmit \
 *         --dsn IBMUSER.LIBC370.CRTSCR
 * Install: jcl/recvcrt.jcl.   Run: jcl/tstcrt.jcl.
 *
 * (mvs/link.h and mvs/thread.h include headers that fail -Werror - a
 * nested comment and #pragma pack, #414/#415 - so no -Werror here.)
 *
 * mvsdev JOB01370, 2026-10-05 (RECEIVE JOB01369): TSTCRTN 1/1, TSTCRTT
 * 4/4, TSTCRTN0 1/1, TSTCRTT0 4/4, all CC 0000; red: TSTCRTNR (old
 * crt0.o) and TSTCRTTR (old crt1.o) CC 0001, 0/1 each.
 *
 * Since 2.4.0 the tree builds no crt0.o: the N0/T0 variants were measured
 * with 2.3.x, which still shipped it as a copy of the @@CRT0 member.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <mvs/link.h>
#ifdef WITH_THREADS
#include <mvs/thread.h>
#include <mvs/ecb.h>
#endif

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

#ifdef WITH_THREADS
#define JOINMAX 6000            /* 6000 x 10 ms yield = 60 s */

static int worker(void *a, void *b)
{
    (void)a;
    (void)b;
    return 7;
}
#endif

int main(void)
{
    CDE *cde = clib_find_cde("CTHREAD");

#ifndef WITH_THREADS
    printf("=== tstcrt: a program without threads (#159) ===\n");
    CHECK(cde == NULL, "the startup did not IDENTIFY CTHREAD");
#else
    CTHDTASK *t;
    int i;
    int rc;

    printf("=== tstcrt: a program with threads (#159) ===\n");
    CHECK(cde != NULL, "the startup IDENTIFYed CTHREAD before main()");
    if (cde != NULL) {
        /* no clib_identify_cthread(), no IDENTIFY of our own */
        t = cthread_create((void *)worker, NULL, NULL);
        CHECK(t != NULL, "cthread_create() attached the subtask");
        if (t != NULL) {
            for (i = 0; i < JOINMAX && !(t->termecb & ECB_POSTED_BIT); i++)
                cthread_yield();
            CHECK(t->termecb & ECB_POSTED_BIT, "the subtask ended");
            rc = t->rc;
            CHECK(rc == 7, "the thread function's return code came back");
            cthread_delete(&t);
        }
    }
    else {
        printf("  (no CTHREAD entry: an ATTACH EP=CTHREAD would abend, "
               "so no thread is created)\n");
    }
#endif

    printf("=== tstcrt: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0)
        printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return (mbt_failed > 0 ? 1 : 0);
}
