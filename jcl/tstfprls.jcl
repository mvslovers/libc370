//TSTFPRLS JOB (SYS),'LIBC370 167 RLSE',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #167 - RLSE through fopen(), measured.
//*
//* The host test (test/host/tstfprls.c) pins that the DALRLSE text unit is
//* built and only when asked.  This job answers the two things it cannot:
//* whether SVC 99 ACCEPTS DALRLSE with DISP=OLD and no space keys - the
//* shape mvslovers/ftpd hits - and whether MVS then RELEASES the space at
//* CLOSE.
//*
//* Every case allocates TRK(30,5), writes ONE record, closes, and reads the
//* format-1 DSCB back with OBTAIN.  30 tracks retained = nothing released.
//*
//* The work data set must NOT exist when the job starts; the probe creates
//* and deletes it.  PARM overrides the name (default IBMUSER.TSTFPRLS.WORK).
//*
//* The probe prints COULD NOT MEASURE rather than a red verdict when the
//* volume does not let it count tracks - a probe that could not measure must
//* not print a verdict.
//*
//* STEPLIB is the scratch PDS the RECEIVE in recvfprl.jcl restores into.
//*
//S1       EXEC PGM=TSTFPRLS,REGION=4096K
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.RLSSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//
