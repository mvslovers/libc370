//TSTJSUB  JOB (SYS),'LIBC370 79',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #79 - JESJOB carries the submit time (JCTRDRON/JCTRDTON)
//* and the input processor's system id (JCTRDSID).  See
//* test/mvs/tstjsub.c.  Reads the JES2 checkpoint and spool only.
//*
//* STEPLIB is the scratch PDS recvjsub.jcl restores into.
//*
//* First submit tstjhold.jcl (TYPRUN=HOLD), wait a while, release it
//* ($AJnnnn), and only then this job: TSTJHOLD's submit time must lie
//* before its start.
//*
//* Run:     mvsdev JOB01066, CC 0000, 2026-10-01: TSTJHOLD JOB01065,
//*          held 20 s, submitted 03:06:18 and started 03:06:39 as its
//*          $HASP373 says; sysid DEV1 as $HASP373's SYS DEV1.
//*
//* RC 0 = every check passed, 8 = a check failed or no job found.
//*
//RUN      EXEC PGM=TSTJSUB,REGION=4M,PARM='TSTJHOLD,TSTJSUB'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.JSUBSCR
//HASPCKPT DD  DISP=SHR,DSN=SYS1.HASPCKPT,UNIT=3350,VOL=SER=MVS000
//HASPACE1 DD  DISP=SHR,DSN=SYS1.HASPACE,UNIT=3350,VOL=SER=SPOOL1
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
