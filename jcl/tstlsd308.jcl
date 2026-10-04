//TSTL308  JOB (SYS),'LIBC370 308',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #308 - __listds() on real LISTCAT output, fixed and previous
//* library side by side.  See test/mvs/tstlsd308.c.  STEPLIB is the
//* scratch PDS recvl308.jcl restores into.  Compare the two SYSPRINTs.
//*
//* mvsdev JOB01340: identical but for LEVEL('SYS1') VOLUME, where the
//* previous library lost PARMLIB, SVCLIB and VTAMLIB.
//*
//NEW      EXEC PGM=TSTL308,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.L308SCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//OLD      EXEC PGM=TSTL308R,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.L308SCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//
