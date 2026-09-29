//TSTDBLCV JOB (SYS),'LIBC370 209',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #209 - printf of a double with a precision above ~14
//* printed a rounding error of its own (5e-15).  See
//* test/mvs/tstdblcv.c.
//*
//* STEPLIB is the scratch PDS recvdbl.jcl restores into.
//*
//* Run:     mvsdev JOB00714, CC 0000, 29/29, 2026-09-29.  Red
//*          against the pre-fix libc: JOB00712, CC 0001, 22 failed.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//RUN      EXEC PGM=TSTDBLCV,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.DBLSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
