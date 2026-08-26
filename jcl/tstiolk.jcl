//TSTIOLK  JOB (SYS),'LIBC370 145',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #145 - vvprintf's nested public fputs() releases the FILE
//* lock; concurrent printf corrupts the stream (ftpd#117 S001 shape).
//* Round 1 measures ENQ/DEQ RET=HAVE nesting semantics, round 2 pins
//* the nested-release mechanism deterministically, round 3 runs two
//* cthread writers against one FILE and reads the dataset back.
//* OUTDD/OUTDD2 are the scratch datasets the rounds write and verify.
//* TIME=1 guards the ENQ waits.  See test/mvs/tstiolk.c.
//*
//S1       EXEC PGM=TSTIOLK,REGION=4M,TIME=1
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//OUTDD    DD  DSN=&&IOLK,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(15,15)),
//             DCB=(RECFM=FB,LRECL=80,BLKSIZE=3120)
//OUTDD2   DD  DSN=&&IOLK2,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(2,2)),
//             DCB=(RECFM=FB,LRECL=80,BLKSIZE=3120)
