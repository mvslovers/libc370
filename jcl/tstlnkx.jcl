//TSTLNKX  JOB (SYS),'LIBC370 553',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #553 - completion codes of __linkx(), __loadx().
//* See test/mvs/tstlnkx.c.
//* GREEN is this tree; RED, where there is one, the installed library.
//*
//GREEN    EXEC PGM=TSTLNKXG,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.LKXSCR
//RED      EXEC PGM=TSTLNKXR,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.LKXSCR
//
