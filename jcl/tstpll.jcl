//TSTPLL   JOB (SYS),'LIBC370 321',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #321 - %lld / %jd print negative values with their sign.
//* See test/mvs/tstpll.c.  STEPLIB is the scratch PDS recvpll.jcl
//* restores into.  TSTPLL links the fixed libc.a, TSTPLLR the 2.0.0
//* sysroot libc.a - the red control.
//*
//* Run:     mvsdev JOB01165, 2026-10-02: GREEN CC 0000, 17/17;
//*          RED CC 0001, 11 of 17 failed.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTPLL,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PLLSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTPLLR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PLLSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
