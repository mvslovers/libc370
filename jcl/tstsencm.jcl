//TSTSENCM JOB (SYS),'LIBC370 475',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #475 - setenv() by the exact name.  See test/mvs/tstsencm.c.
//* GREEN is this tree; RED, where there is one, the installed library.
//*
//GREEN    EXEC PGM=TSTSENCG,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SCSCR
//RED      EXEC PGM=TSTSENCR,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SCSCR
//
