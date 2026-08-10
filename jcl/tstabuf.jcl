//TSTABUF  JOB (SYS),'LIBC370 90',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #90 - @@AOPEN's buffer-1 cleanup on the VBS-area
//* GETMAIN-failure path assembled to NO code (IHB019 MNOTE
//* swallowed), so the #83 failure path leaked the buffer exactly
//* when storage was short.  SYSUT1's LRECL(27994) >> BLKSIZE(3120)
//* makes the VBS record area unobtainable in the squeezed region
//* while buffer 1 fits: pre-fix each failing open eats a 4K slack
//* hole (RC 8), post-fix the slack survives intact (COND CODE 0000).
//* See test/mvs/tstabuf.c.
//*
//S1       EXEC PGM=TSTABUF,REGION=1024K
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//SYSUT1   DD  UNIT=SYSDA,SPACE=(TRK,(4,2)),
//             DCB=(RECFM=VS,LRECL=27994,BLKSIZE=3120,DSORG=PS)
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
