//TSTTSOM  JOB (SYS),'LIBC370 413',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #413 - tsocmd() and the caller's ECT.  See test/mvs/tsttsom.c.
//* GREEN is this tree, RED the installed library; each runs as a TSO
//* command in a batch TSO session.
//*
//GREEN    EXEC PGM=IKJEFT01,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.TTSCR
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  TSTTSOG
/*
//RED      EXEC PGM=IKJEFT01,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.TTSCR
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  TSTTSOR
/*
//
