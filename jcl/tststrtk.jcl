//TSTSTRTK JOB (SYS),'LIBC370 STRTOK',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 - library calls keep the caller's strtok() position.
//* See test/mvs/tststrtk.c.  STEPLIB is the scratch PDS recvstk.jcl
//* restores into.  TSTSTK links the fixed libc.a, TSTSTKR the
//* installed sysroot libc.a - the red control.
//*
//* Run:     mvsdev JOB01360, 2026-10-04: GREEN 5/5; RED 1/5.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTSTK,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.STKSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTSTKR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.STKSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//
