/* OSCHECK.C - wait for io completion */
#include "mvs/osio.h"
#include "mvs/recovery.h"

static void check(DECB *decb, int *state) __attribute__((noinline));

/* the library's EODAD and SYNAD, at the end of this file (#412) */
extern void __oseod(void)   asm("@@OSEOD");
extern void __ossyn(void)   asm("@@OSSYN");

void __osdfx(DCB *dcb, int eodad)           asm("@@OSDFX");

/* addr into the 3-byte address of an exit field that names no routine:
   0, or the 1 the DCB macro leaves in an EODAD that was not coded */
static void
dfltexit(void **field, void (*addr)(void))
{
    unsigned    old = (unsigned)*field;

    if ((old & 0x00FFFFFF) > 1) return;
    *field = (void *)((old & 0xFF000000) | ((unsigned)addr & 0x00FFFFFF));
}

/* the library's SYNAD, and its EODAD when eodad is set, into the DCB
   that osbdcb() or osddcb() has just built (#412) */
void
__osdfx(DCB *dcb, int eodad)
{
    if (eodad) dfltexit(&dcb->dcbeodad, __oseod);
    dfltexit(&dcb->dcbsynad, __ossyn);
}

int
oscheck(DECB *decb)
{
    int rc;
    int state = 0;      /* set by the library's EODAD and SYNAD (#412) */

    /* we use try() to catch any abends */
    rc = try(check, decb, &state);
    if (rc) return rc;

    /* 4: end of data, through the library's EODAD.  8: an I/O error,
       through the library's SYNAD - or through a SYNAD of the caller's
       that returned, which leaves the completion code in the ECB.  A
       request that went well leaves X'7F' (BSAM) or X'40' with no
       exception code (BDAM, measured on MVS) (#412). */
    if (state) return state;
    if ((decb->decsdecb >> 24) == 0x7F) return 0;
    if ((decb->decsdecb >> 8) == 0x400000) return 0;
    return 8;
}

/* The CHECK, and the default EODAD and SYNAD routines osbdcb() and
** osddcb() put into a DCB that names none (#412).  CHECK enters both
** exits with R2-R13 as they were at the CHECK, and the EODAD by a branch
** that does not return to it (knowledge MVS-BSAM-0002).  So R2 carries a
** mark and R3 the address of state: an exit taken from this CHECK sets
** state and goes on after it.  Taken from a CHECK of the caller's own,
** without the mark, the EODAD ends in the abend the DCB without an EODAD
** gave (S337), and the SYNAD returns, as a SYNAD that does nothing. */
__asm__("\n&FUNC    SETC 'check'");
static void
check(DECB *decb, int *state)
{
    __asm__(
"         LR    1,%0\n"
"         LR    3,%1\n"
"         L     2,=X'D6E2C3D2'      'OSCK', the mark\n"
"         CHECK (1)\n"
"         B     OSCKDONE\n"
"         ENTRY @@OSEOD\n"
"@@OSEOD  BALR  15,0               the default EODAD\n"
"         USING *,15\n"
"         C     2,OSCKMARK          from oscheck() ?\n"
"         BNE   OSEODABE            no\n"
"         MVC   0(4,3),OSCKEOD      state = 4, end of data\n"
"         B     OSCKDONE\n"
"OSEODABE ABEND 823,,,SYSTEM       S337, as without an EODAD\n"
"         ENTRY @@OSSYN\n"
"@@OSSYN  BALR  15,0               the default SYNAD\n"
"         USING *,15\n"
"         C     2,OSCKMARK          from oscheck() ?\n"
"         BNER  14                  no, back to CHECK\n"
"         MVC   0(4,3),OSCKIOE      state = 8, I/O error\n"
"         BR    14                  back to CHECK\n"
"         DS    0F\n"
"OSCKMARK DC    X'D6E2C3D2'\n"
"OSCKEOD  DC    F'4'\n"
"OSCKIOE  DC    F'8'\n"
"         DROP  15\n"
"OSCKDONE DS    0H"
        : : "r"(decb), "r"(state) : "0", "1", "2", "3", "14", "15", "memory");

    return;
}
