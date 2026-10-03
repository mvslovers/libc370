//TSTVSTD  JOB (SYS),'LIBC370 338',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #338 - vprintf(), vscanf(), vfscanf().  See
//* test/mvs/tstvstd.c.  STEPLIB is the scratch PDS recvvstd.jcl
//* restores into; SYSIN holds the input the test reads.
//*
//* Run:     mvsdev JOB01310, 2026-10-03: GREEN CC 0000, 5/5.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTVSTD,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.VSTDSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//SYSIN    DD  *
42 -7
ff 3.5
/*
