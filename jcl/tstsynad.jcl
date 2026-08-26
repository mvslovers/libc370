//TSTSYNAD JOB (SYS),'LIBC370 147',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #147 item 3 - no SYNAD on the DCBs @@AOPEN builds, so a
//* genuine I/O error is ABEND S001 for the whole address space.
//* Round 1 writes &&BAD as FB/3120; round 2 reads the same dataset
//* through BADIN, whose DCB override claims BLKSIZE=80 - the first
//* READ meets a 3120-byte block, a wrong-length I/O error.  RED =
//* S001 (caught by try(), reported in the job log); GREEN = ferror()
//* + errno EIO and the program runs on.  See test/mvs/tstsynad.c.
//*
//S1       EXEC PGM=TSTSYNAD,REGION=4M,TIME=1
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//OUT80    DD  DSN=&&BAD,DISP=(NEW,PASS),UNIT=SYSDA,
//             SPACE=(TRK,(5,5)),
//             DCB=(RECFM=FB,LRECL=80,BLKSIZE=3120)
//BADIN    DD  DSN=&&BAD,DISP=(OLD,DELETE),VOL=REF=*.OUT80,
//             DCB=(RECFM=F,LRECL=80,BLKSIZE=80)
