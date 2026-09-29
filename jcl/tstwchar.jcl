//TSTWCHAR JOB (SYS),'LIBC370 195',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #195 / #213 - wchar_t, wint_t and ptrdiff_t are cc370's
//* types, and the multibyte conversions work with them.  See
//* test/mvs/tstwchar.c.
//*
//* STEPLIB is the scratch PDS recvwch.jcl restores into.  TSTWCH is
//* linked against the current libc.a, TSTWCR against libc 8928b2a
//* (before #195) - the red control.  Both are compiled with the new
//* headers; the old ones do not compile the probe at all.
//*
//* Run:     mvsdev JOB00649, 2026-09-29: GREEN CC 0000, 18/18;
//*          RED CC 0001, 11 of 18 failed.
//*          With (5) for cc370#484 (cc370 6ba027d): JOB00665,
//*          GREEN CC 0000, 22/22; RED CC 0001, the same 11 failed.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTWCH,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.WCHSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTWCR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.WCHSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
