//TSTAOPN  JOB (SYS),'LIBC370 83',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #83 - fopen()/dynalloc still abended S878 on storage
//* shortage: unconditional GETMAINs in the FUNHEAD SAVE= path,
//* @@AOPEN and @@SVC99.  This job exhausts its own region and calls
//* __aopen()/__svc99() directly: pre-fix it abends S878 in the
//* FUNHEAD save area GETMAIN, post-fix both fail cleanly and the
//* same open succeeds after free-all.  See test/mvs/tstaopn.c.
//*
//S1       EXEC PGM=TSTAOPN,REGION=1024K
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//SYSUT1   DD  UNIT=SYSDA,SPACE=(TRK,(2,1)),
//             DCB=(RECFM=FB,LRECL=80,BLKSIZE=3120,DSORG=PS)
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
