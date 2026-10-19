//TSTBSRM  JOB (SYS),'LIBC370 388',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #388 - bsearch() calls compar(key, elem).  test/mvs/tstbsrm.c
//* GREEN is this tree; RED, where there is one, the installed library.
//*
//GREEN    EXEC PGM=TSTBSRG,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.BSSCR
//RED      EXEC PGM=TSTBSRR,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.BSSCR
//
