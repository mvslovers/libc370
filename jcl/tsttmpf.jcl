//TSTTMPF  JOB (SYS),'LIBC370 395',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #395 item 3 - tmpfile() reads back what was written.  See
//* test/mvs/tsttmpf.c.  STEPLIB is the scratch PDS recvtmp.jcl
//* restores into; tmpfile() allocates its own &&TMP data sets.
//*
//* GREEN is this tree's libc.a (CC 0000 expected), RED the installed
//* one (CC 0001 expected: fread() after rewind() returns 0).
//*
//* Run:     mvsdev JOB01589, 2026-10-07: GREEN CC 0000, 7/7; RED CC 0001,
//*          3/7.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTTMP,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.TMPSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTTMPR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.TMPSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
