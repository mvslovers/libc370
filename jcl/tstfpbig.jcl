//TSTFPBIG JOB (SYS),'LIBC370 385',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #385 - fprintf() of 8192 characters and more.  See
//* test/mvs/tstfpbig.c.  STEPLIB is the scratch PDS recvfpb.jcl
//* restores into.  BIG1-3 are temporary FB 80 data sets per step.
//*
//* GREEN is this tree's libc.a (CC 0000 expected), RED the installed
//* one (CC 0001 expected: cut at 8192, last byte X'00', count 0 on error).
//*
//* Run:     mvsdev JOB01544, 2026-10-06: GREEN CC 0000, 8/8; RED CC 0001,
//*          2/8.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTFPB,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.FPBSCR
//BIG1     DD  DSN=&&BIG1G,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(5,5)),DCB=(RECFM=FB,LRECL=80,BLKSIZE=800)
//BIG2     DD  DSN=&&BIG2G,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(5,5)),DCB=(RECFM=FB,LRECL=80,BLKSIZE=800)
//BIG3     DD  DSN=&&BIG3G,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(5,5)),DCB=(RECFM=FB,LRECL=80,BLKSIZE=800)
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTFPBR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.FPBSCR
//BIG1     DD  DSN=&&BIG1R,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(5,5)),DCB=(RECFM=FB,LRECL=80,BLKSIZE=800)
//BIG2     DD  DSN=&&BIG2R,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(5,5)),DCB=(RECFM=FB,LRECL=80,BLKSIZE=800)
//BIG3     DD  DSN=&&BIG3R,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(5,5)),DCB=(RECFM=FB,LRECL=80,BLKSIZE=800)
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
