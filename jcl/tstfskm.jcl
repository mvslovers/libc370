//TSTFSKM  JOB (SYS),'LIBC370 178',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #178 - fseek() keeps the error indicator, rewind() clears
//* it.  See test/mvs/tstfskm.c.  GREEN is this tree, RED a build of
//* the library before the fix.  RC 0 passed, 8 failed, 12 no test.
//*
//* Run: own MVS/CE container, 2026-10-20: GREEN CC 0000 (17/17),
//*      RED CC 0008 (4 FAIL: the error indicator was cleared).
//*
//GREEN    EXEC PGM=TSTFSKG,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.FSKSCR
//WORK     DD  DSN=&&WORKG,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(5,5)),DCB=(RECFM=FB,LRECL=80,BLKSIZE=800)
//FULL     DD  DSN=&&FULLG,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,0)),DCB=(RECFM=FB,LRECL=80,BLKSIZE=800)
//*
//RED      EXEC PGM=TSTFSKR,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.FSKSCR
//WORK     DD  DSN=&&WORKR,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(5,5)),DCB=(RECFM=FB,LRECL=80,BLKSIZE=800)
//FULL     DD  DSN=&&FULLR,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,0)),DCB=(RECFM=FB,LRECL=80,BLKSIZE=800)
//
