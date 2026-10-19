//TSTDSCBM JOB (SYS),'LIBC370 409',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #409 - __dscbav() of a binary CCHHR.  test/mvs/tstdscbm.c
//* GREEN is this tree; RED, where there is one, the installed library.
//*
//GREEN    EXEC PGM=TSTDSCBG,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.DBSCR
//RED      EXEC PGM=TSTDSCBR,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.DBSCR
//
