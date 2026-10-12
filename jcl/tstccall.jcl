//TSTCCALL JOB (SYS),'LIBC370 502',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #502 - calling C from code that is not C (@@CCALL).
//* See test/mvs/tstccall.c.  STEPLIB is the scratch PDS recvccal.jcl
//* restores into.
//*
//*   GREEN  TSTCCALL: CC 0000, every check PASS.
//*   NOC    TSTCCNOC: CC 0000 (#507).  An assembler main with no C
//*          program: the first call builds the C environment, exit(5)
//*          comes back as RC 5, CCTERM releases it.  SYSPRINT: "call
//*          1", "call 2", "atexit ran", "call 3".  RC 11-71 name the
//*          check that failed (test/mvs/tstccnoc.asm).
//*   NOT    TSTCCNOT: no CCTERM - its "call 1" line never reaches
//*          SYSPRINT (the control for CCTERM).
//*   RED    TSTCCALR: the C function called directly, without
//*          @@CCALL: a program check, never CC 0000.
//*
//* Run:     dev1 JOB02052, 2026-10-12 (#502): GREEN CC 0000, 6/6;
//*          NOC U3101 (before #507); RED S0C4.
//*          dev1 JOB02062 (#507): GREEN 6/6; NOC CC 0000, all four
//*          lines; NOT CC 0000, SYSPRINT empty; RED S0C4.
//*
//GREEN    EXEC PGM=TSTCCALL,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.CCSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//NOC      EXEC PGM=TSTCCNOC,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.CCSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//NOT      EXEC PGM=TSTCCNOT,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.CCSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTCCALR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.CCSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
