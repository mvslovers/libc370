//TSTPLEN  JOB (SYS),'LIBC370 PLEN',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* httpd#275 - a parm length of 1..3 with a zero third byte is not
//* TSO-shaped.  See test/mvs/tstplen.c.  TSTPLEN LINKs the target named
//* in PARM with R1 -> A(X'0002',X'00','X').  TSTPLNT links this tree's
//* libc.a, TSTPLNR the installed one (2.4.0) - the red control.
//*
//* Run:     mvsdev JOB01408, 2026-10-05: GREEN CC 0, RED CC 1.
//*
//* RC 0 = the target ran and returned 7, 1 = it did not.
//*
//GREEN    EXEC PGM=TSTPLEN,REGION=4M,PARM='TSTPLNT'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PLNSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTPLEN,REGION=4M,PARM='TSTPLNR',COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PLNSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
