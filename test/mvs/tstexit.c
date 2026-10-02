/*
 * tstexit.c - libc370 #314 on MVS: _Exit() ends the program without
 * running atexit() functions, and still closes the streams.
 *
 * The verdict is in the job output, since nothing runs after _Exit():
 *
 *   TSTEXIT  (_Exit(7))  COND CODE 0007; SYSPRINT shows BEFORE and
 *                        does NOT show ATEXIT RAN
 *   TSTEXITC (exit(7))   COND CODE 0007; SYSPRINT shows BEFORE and
 *                        ATEXIT RAN - the control that the handler is
 *                        registered and would print
 *
 * BEFORE showing in both says the stream was written out: _Exit() leaves
 * the teardown, file close included, to __exit().
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstexit.c -o TSTEXIT -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk -DCONTROL \
 *                test/mvs/tstexit.c -o TSTEXITC -flinker-output=iebcopy
 *          ld370 --pack TSTEXIT=TSTEXIT.iebcopy TSTEXITC=TSTEXITC.iebcopy \
 *                -o tstexit -xmit --dsn IBMUSER.LIBC370.EXITSCR
 * Install: jcl/recvexit.jcl.   Run: jcl/tstexit.jcl.
 *
 * mvsdev JOB01169, 2026-10-02 (RECEIVE JOB01168): both steps COND CODE
 * 0007; EXIT's SYSPRINT is BEFORE alone, CONTROL's BEFORE + ATEXIT RAN.
 */
#include <stdio.h>
#include <stdlib.h>

static void handler(void)
{
    printf("ATEXIT RAN\n");
}

int main(void)
{
    if (atexit(handler) != 0) {
        printf("atexit() failed\n");
        return 1;
    }
    printf("BEFORE\n");
#ifdef CONTROL
    exit(7);
#else
    _Exit(7);
#endif
}
