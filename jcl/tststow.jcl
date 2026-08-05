//TSTSTOW  JOB (SYS),'LIBC370 32',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #32 - __stow() emitted no instruction; every PDS directory
//* operation was a silent no-op returning the func letter (rc=195).
//* Needs the PDS with members M1 and M9 - see test/mvs/tststow.c.
//*
//S1       EXEC PGM=TSTSTOW,REGION=4M,PARM='IBMUSER.STOWTEST.PDS'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
