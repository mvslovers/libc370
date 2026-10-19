//TSTFCLM  JOB (SYS),'LIBC370 174',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #174 - fclose() leaves no ENQ behind.  See test/mvs/tstfclm.c.
//* GREEN is this tree; RED, where there is one, the installed library.
//*
//GREEN    EXEC PGM=TSTFCLG,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.FCSCR
//RED      EXEC PGM=TSTFCLR,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.FCSCR
//
