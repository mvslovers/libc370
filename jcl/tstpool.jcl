//TSTPOOL  JOB (SYS),'LIBC370 700',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #700 - a pool of 1-3 workers gets a worker.
//* #708 - a pool of 0 workers is refused (NULL, EINVAL).
//* See test/mvs/tstpool.c.  GREEN is this tree, RED the library
//* built from main before the fix.
//*
//GREEN    EXEC PGM=TSTPLG,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.POOLSCR
//RED      EXEC PGM=TSTPLR,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.POOLSCR
//
