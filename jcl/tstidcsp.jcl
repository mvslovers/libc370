//TSTIDCSP JOB (SYS),'LIBC370 71',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #71 - idcams_sysprint() on a real IDCAMS: every SYSPRINT
//* line with its message number.  See test/mvs/tstidcsp.c.  Every
//* command names a data set under IBMUSER.TSTIDCSP that does not
//* exist, so nothing on the system changes.
//*
//* STEPLIB is the scratch PDS recvidcs.jcl restores into.
//*
//* Run:     mvsdev JOB01060, CC 0000, 2026-10-01.  The first run
//*          (JOB01058, CC 0008) measured that IDCAMS hands its exit
//*          IDC3012I as 12 and IDC0001I/IDC0002I as -1/-2; libc370 now
//*          reads the number from the record.
//*
//* RC 0 = every check passed, 8 = a check failed.
//*
//RUN      EXEC PGM=TSTIDCSP,REGION=4M,PARM='IBMUSER'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.IDCSSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
