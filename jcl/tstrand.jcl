//TSTRAND  JOB (SYS),'LIBC370 387',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #387 - rand() within RAND_MAX, default seed 1.  See
//* test/mvs/tstrand.c.  STEPLIB is the scratch PDS recvrnd.jcl
//* restores into.
//*
//* GREEN is this tree's libc.a (CC 0000 expected), RED the installed
//* one (CC 0001 expected: DEFAULT, SRAND1 and RANGE fail).
//*
//* Run:     mvsdev JOB01712, 2026-10-08 (RECEIVE JOB01711): GREEN CC 0000,
//*          5/5; RED (installed 2.6.2) CC 0001, 1/5.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTRND,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.RNDSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTRNDR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.RNDSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
