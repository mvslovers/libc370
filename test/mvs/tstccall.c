/*
 * tstccall.c - libc370 #502 on MVS: calling C from code that is not C.
 *
 * Code that is not C (COBOL, assembler) calls with an OS save area of
 * 72 bytes in R13; a C function takes its frame from the word at 76 of
 * it.  @@CCALL, reached through a CCALL stub, finds the C frame that
 * called out and calls the C function from a frame at its NAB.
 *
 * The non-C callers are in test/mvs/tstccall.asm: OS-linkage routines
 * with a STATIC save area in the load module, the way COBOL code has
 * one, below the C stack.  Checked:
 *
 *   (1) C -> OSADD -> CCADD -> @@CCALL -> CADD: the value and the return
 *       code arrive, and CADD gets its pointer without the X'80' that
 *       marks the last address of the list
 *   (2) nested: C -> OSNEST -> CCNEST -> CNEST -> OSADD -> CCADD -> CADD
 *   (3) OSSPIE establishes a SPIE, as COBOL does.  Inside CSPIE no SPIE
 *       is in force, a 0C1 under try() comes back as 0x000C1000, and
 *       after the call OSSPIE finds its own SPIE in force again
 *   (4) no SPIE in force once OSSPIE is done
 *
 * TSTCCNOC and TSTCCNOT (test/mvs/tstccnoc.asm, .c) are the other case,
 * an assembler main with no C program on the task (#507): the first call
 * builds the C environment, exit() and CCTERM release it.
 *
 * RED CONTROL: TSTCCALR is this source with -DRED: (1) calls CADD
 * directly, without @@CCALL, as COBOL code does today.  The C prologue
 * takes the word behind OSDIRECT's save area for the NAB; the step ends
 * in a program check or worse, never with CC 0.
 *
 * Build:   make build
 *          as370 -I maclib test/mvs/tstccall.asm -o tstccala.o
 *          cc370 -Os -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstccall.c tstccala.o -o TSTCCALL \
 *                -flinker-output=iebcopy
 *          cc370 -Os -Wall -Werror -DRED -Iinclude -L build/sdk \
 *                test/mvs/tstccall.c tstccala.o -o TSTCCALR \
 *                -flinker-output=iebcopy
 *          as370 -I maclib test/mvs/tstccnoc.asm -o tstccnoc.o
 *          cc370 -c -Os -Wall -Werror -Iinclude test/mvs/tstccnoc.c \
 *                -o tstccnoc_c.o
 *          cc370 tstccnoc.o tstccnoc_c.o -L build/sdk -o TSTCCNOC \
 *                -Wl,--entry,TSTCCNOC -flinker-output=iebcopy
 *          cc370 tstccnoc.o tstccnoc_c.o -L build/sdk -o TSTCCNOT \
 *                -Wl,--entry,TSTCCNOT -flinker-output=iebcopy
 *          ld370 --pack TSTCCALL=TSTCCALL.iebcopy \
 *                --pack TSTCCALR=TSTCCALR.iebcopy \
 *                --pack TSTCCNOC=TSTCCNOC.iebcopy \
 *                --pack TSTCCNOT=TSTCCNOT.iebcopy \
 *                -o tstccall -xmit --dsn IBMUSER.LIBC370.CCSCR
 * Install: jcl/recvccal.jcl.   Run: jcl/tstccall.jcl.
 *
 * dev1 JOB02052, 2026-10-12 (RECEIVE JOB02051): GREEN CC 0000, 6/6;
 * NOC U3101 with its message; RED S0C4.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <mvs/recovery.h>

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

#define VL(p) ((void *)((unsigned)(p) | 0x80000000))

extern int osadd(void *n) asm("OSADD");
extern int osnest(void *n) asm("OSNEST");
extern int osspie(void *n) asm("OSSPIE");
extern int osdirect(void *n) asm("OSDIRECT");
extern void *getpica(void) asm("GETPICA");

int cadd(int *n) asm("CADD");
int cnest(int *n) asm("CNEST");
int cspie(int *n) asm("CSPIE");

/* +100, rc 9; rc 99 when the pointer still carries the X'80' */
int cadd(int *n)
{
    if ((unsigned)n & 0x80000000)
        return 99;
    *n += 100;
    return 9;
}

/* +10, then C -> OSADD -> @@CCALL -> CADD from inside a C function that
   was itself called through @@CCALL; rc one more than CADD's */
int cnest(int *n)
{
    if ((unsigned)n & 0x80000000)
        return 99;
    *n += 10;
    return osadd(VL(n)) + 1;
}

/* an operation exception: a branch to data holding the opcode X'00' */
static const unsigned short badop[2] = { 0x0000, 0x0000 };

static int boom(int x)
{
    int (*f)(int) = (int (*)(int))(void *)badop;
    return f(x);
}

static void *pica_in = (void *)-1;

/* the SPIE in force inside, and a 0C1 under try() */
int cspie(int *n)
{
    pica_in = getpica();
    *n = try(boom, 1);
    return 5;
}

int main(void)
{
    int n, rc;
    char msg[96];

    printf("TSTCCALL - calling C from code that is not C (#502)\n");

#ifdef RED
    n = 1;
    rc = osdirect(VL(&n));
    printf("RED: CADD called directly returned rc=%d n=%d\n", rc, n);
    return 1;
#endif

    n = 1;
    rc = osadd(VL(&n));
    sprintf(msg, "C -> OS routine -> @@CCALL -> C: rc=%d n=%d (9, 101)", rc, n);
    CHECK(rc == 9 && n == 101, msg);

    n = 1;
    rc = osnest(VL(&n));
    sprintf(msg, "nested C -> OS -> C -> OS -> C: rc=%d n=%d (10, 111)", rc, n);
    CHECK(rc == 10 && n == 111, msg);

    n = -1;
    rc = osspie(VL(&n));
    sprintf(msg, "caller's SPIE in force again after the call: rc=%d (5; 98 = not)", rc);
    CHECK(rc == 5, msg);
    sprintf(msg, "no SPIE in force inside the C function: PICA %08X", (unsigned)pica_in);
    CHECK(pica_in == 0, msg);
    sprintf(msg, "0C1 in the C function reaches try(): %08X (000C1000)", (unsigned)n);
    CHECK((unsigned)n == 0x000C1000, msg);

    sprintf(msg, "no SPIE in force afterwards: PICA %08X", (unsigned)getpica());
    CHECK(getpica() == 0, msg);

    printf("TSTCCALL %d run, %d passed, %d failed\n", mbt_run, mbt_passed, mbt_failed);
    return mbt_failed ? 1 : 0;
}
