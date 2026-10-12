//TSTCCALL JOB (SYS),'LIBC370 502',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #502 - calling C from code that is not C (@@CCALL).
//* See test/mvs/tstccall.c.  STEPLIB is the scratch PDS recvccal.jcl
//* restores into.
//*
//*   GREEN  TSTCCALL: CC 0000, every check PASS.
//*   NOC    TSTCCNOC: ABEND U3101 and "@@CCALL - no C program on this
//*          task" - an assembler main, no C program on the task.
//*   RED    TSTCCALR: the C function called directly, without
//*          @@CCALL: a program check, never CC 0000.
//*
//* Run:     dev1 JOB02052, 2026-10-12: GREEN CC 0000, 6/6;
//*          NOC U3101; RED S0C4.
//*
//GREEN    EXEC PGM=TSTCCALL,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.CCSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//NOC      EXEC PGM=TSTCCNOC,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.CCSCR
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTCCALR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.CCSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
