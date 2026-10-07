//TSTFLOCK JOB (SYS),'LIBC370 453',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #453 - the FILE lock costs nothing until a thread exists.
//* See test/mvs/tstflock.c.  STEPLIB is the scratch PDS recvflk.jcl
//* restores into.  TXT and LOG are temporary data sets per step.
//*
//* GREEN is this tree's libc.a (CC 0000 expected), RED the installed
//* one (CC 0001 expected: FAST fails, about 126 us a call).
//*
//* Run:     mvsdev JOB01598, 2026-10-07: GREEN CC 0000, 11/11; RED CC 0001,
//*          9/11.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTFLK,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.FLKSCR
//TXT      DD  DSN=&&TXTG,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(5,5)),DCB=(RECFM=FB,LRECL=80,BLKSIZE=800)
//LOG      DD  DSN=&&LOGG,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(5,5)),DCB=(RECFM=FB,LRECL=80,BLKSIZE=800)
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTFLKR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.FLKSCR
//TXT      DD  DSN=&&TXTR,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(5,5)),DCB=(RECFM=FB,LRECL=80,BLKSIZE=800)
//LOG      DD  DSN=&&LOGR,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(5,5)),DCB=(RECFM=FB,LRECL=80,BLKSIZE=800)
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
