//TSTRSEEK JOB (SYS),'LIBC370 473',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #473 - fseek() inside the buffer of an "r" stream.  See
//* test/mvs/tstrseek.c.  STEPLIB is the scratch PDS recvrsk.jcl
//* restores into.  TXT is a temporary FB 80 data set per step.
//*
//* GREEN is this tree's libc.a (CC 0000 expected), RED the installed
//* one (CC 0001 expected: SEEK0, TELL, BACK1 and FWD fail).
//*
//* Run:     mvsdev JOB01675, 2026-10-07: GREEN CC 0000, 8/8; RED CC 0001,
//*          3/8 (SEEK0, TELL, BACK1, FWD).
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTRSK,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.RSKSCR
//TXT      DD  DSN=&&TXTG,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,1)),DCB=(RECFM=FB,LRECL=80,BLKSIZE=800)
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTRSKR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.RSKSCR
//TXT      DD  DSN=&&TXTR,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,1)),DCB=(RECFM=FB,LRECL=80,BLKSIZE=800)
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
