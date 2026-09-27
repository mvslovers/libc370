//TSTWRPOS JOB (SYS),'LIBC370 189 WRPOS',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #189 - three write-path defects: an empty line on a text
//* stream, ftell() after writes, fseek() on a write stream.
//*
//* Measures, decides nothing.  Grep the job log for TSTWRPOS.
//* CC 0000 = every case was measured, whatever the verdicts are.
//*
//CLEAN    EXEC PGM=IDCAMS
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  DELETE IBMUSER.LIBC370.T189.FB PURGE
  DELETE IBMUSER.LIBC370.T189.VB PURGE
  SET MAXCC=0
/*
//*
//ALLOC    EXEC PGM=IEFBR14
//FB       DD  DSN=IBMUSER.LIBC370.T189.FB,DISP=(NEW,CATLG),
//             UNIT=SYSDA,SPACE=(TRK,(2,1)),
//             DCB=(RECFM=FB,LRECL=80,BLKSIZE=800)
//VB       DD  DSN=IBMUSER.LIBC370.T189.VB,DISP=(NEW,CATLG),
//             UNIT=SYSDA,SPACE=(TRK,(2,1)),
//             DCB=(RECFM=VB,LRECL=84,BLKSIZE=800)
//*
//S1       EXEC PGM=TSTWRPOS,REGION=4096K
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.T189SCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//
