//TSTWALKP JOB (SYS),'LIBC370 80',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #80 - __walkpd() on real PDS directories: SYS1.MACLIB
//* against __listpd(), a callback that stops, a filter, a missing data
//* set, and SYS1.SMPCDS.  See test/mvs/tstwalkp.c.  Reads only.
//*
//* STEPLIB is the scratch PDS recvwalk.jcl restores into.
//*
//* Run:     mvsdev JOB01076, CC 0000, 2026-10-01: SYS1.MACLIB 742
//*          members by walk and list alike (41 with IEF*), stop at 10,
//*          SYS1.SMPCDS 23018 members walked without storage.  The first
//*          run (JOB01074) showed fopen() leaves errno 0 for a data set
//*          that does not exist; __walkpd() now sets EIO there.
//*
//* RC 0 = every check passed, 8 = a check failed.
//*
//RUN      EXEC PGM=TSTWALKP,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.WALKSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
