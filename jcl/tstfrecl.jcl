//TSTFRECL JOB (SYS),'LIBC370 559',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #559 - fread() on a record stream returns the number of
//* elements read.  See test/mvs/tstfrecl.c.  STEPLIB is the scratch
//* PDS recvfrl.jcl restores into.  GREEN: this tree's libc.a; RED:
//* the installed one.  Temporary data sets only.
//*
//* Run:     mvsdev JOB02382, 2026-10-16: GREEN CC 0000, 8/8;
//*          RED CC 0008, 4 failed (32760 for every record).
//*
//* RC 0 = every check passed, 8 = a check failed.
//*
//GREEN    EXEC PGM=TSTFRECL,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.FRLSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//RECU     DD  DSN=&&RECU,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,1))
//RECV     DD  DSN=&&RECV,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,1))
//RECF     DD  DSN=&&RECF,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,1))
//*
//RED      EXEC PGM=TSTFRECR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.FRLSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//RECU     DD  DSN=&&RECU,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,1))
//RECV     DD  DSN=&&RECV,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,1))
//RECF     DD  DSN=&&RECF,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,1))
