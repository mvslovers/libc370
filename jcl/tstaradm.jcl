//TSTARADM JOB (SYS),'LIBC370 398',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #398 - arrayaddf() of a string longer than 1023 characters.
//* See test/mvs/tstaradm.c.  GREEN this tree, RED the installed
//* library.
//*
//GREEN    EXEC PGM=TSTARADG,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.ARSCR
//RED      EXEC PGM=TSTARADR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.ARSCR
//
