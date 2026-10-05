//TSTCTWD  JOB (SYS),'LIBC370 431',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #431 - cthread_wait(termecb), then cthread_delete().
//* See test/mvs/tstctwd.c.  STEPLIB is the scratch PDS recvctw.jcl
//* restores into.  TSTCTWD links this tree's libc.a, TSTCTWDR the
//* installed one (2.3.0) - the red control, expected to end SA03.
//*
//* Run:     mvsdev JOB01388, 2026-10-05: GREEN CC 0000, 4/4;
//*          RED ABEND SA03 ("has not ended").
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTCTWD,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.CTWSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTCTWDR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.CTWSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
