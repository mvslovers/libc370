//TSTTODM  JOB (SYS),'LIBC370 390',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #390 - the TOD clock in seconds past 2038.  See test/mvs/tsttodm.c.
//* GREEN is this tree; RED, where there is one, the installed library.
//*
//GREEN    EXEC PGM=TSTTODG,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.TDSCR
//RED      EXEC PGM=TSTTODR,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.TDSCR
//
