//TSTSDWFM JOB (SYS),'LIBC370 640',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #640 - try() frees the SDWA of a retried abend.  See test/mvs/tstsdwfm.c.
//* GREEN is this tree; RED, where there is one, the installed library.
//*
//GREEN    EXEC PGM=TSTSDWFG,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SFSCR
//RED      EXEC PGM=TSTSDWFR,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SFSCR
//
