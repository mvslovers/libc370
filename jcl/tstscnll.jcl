//TSTSCNLL JOB (SYS),'LIBC370 318',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #318 - scanf's length modifiers hh, ll, j, z, t and L.
//* See test/mvs/tstscnll.c.  STEPLIB is the scratch PDS recvscn.jcl
//* restores into.  TSTSCN links the fixed libc.a, TSTSCNR the installed
//* sysroot libc.a (before #318) - the red control.
//*
//* Run:     mvsdev JOB01171, 2026-10-03: GREEN CC 0000, 23/23;
//*          RED CC 0001, 18 of 23 failed.
//*          With #316's cases: JOB01181, GREEN CC 0000, 43/43;
//*          RED ABEND S0CC (scanf's old float parser).
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTSCN,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SCNSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTSCNR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SCNSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
