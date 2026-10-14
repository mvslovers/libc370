//TSTTRYI  JOB (SYS),'LIBC370 530',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #530 - tryinfo(): PSW and R14 of an abend try() caught.
//* See test/mvs/tsttryi.c.  STEPLIB is the scratch PDS recvtryi.jcl
//* restores into.  No red control: tryinfo() is new, a program using
//* it does not link against an older libc.a.
//*
//* Run:     mvsdev JOB02236, 2026-10-14: CC 0000, 6/6 PASS.
//*
//* RC 0 = every check passed, 8 = a check failed (it is the COND CODE).
//*
//TSTTRYI  EXEC PGM=TSTTRYI,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.TRYISCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
