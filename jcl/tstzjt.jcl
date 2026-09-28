//TSTZJT   JOB (SYS),'LIBC370 211',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #211 - the C99 length modifiers z, t, j and hh through
//* snprintf and sprintf.  See test/mvs/tstzjt.c.
//*
//* STEPLIB is the scratch PDS recvzjt.jcl restores into.  TSTZJT is
//* linked against the branch libc.a, TSTZJR against the sysroot libc
//* before the fix - the red control.
//*
//* Run:     mvsdev JOB00640, 2026-09-29: GREEN CC 0000, 24/24;
//*          RED CC 0001, 20 of 24 failed (every z/t/j/hh case).
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTZJT,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.ZJTSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTZJR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.ZJTSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
