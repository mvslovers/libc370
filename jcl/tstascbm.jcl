//TSTASCBM JOB (SYS),'LIBC370 403',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #403 - __ascb() of the highest ASID.  See
//* test/mvs/tstascbm.c.  Run while started tasks of TSTASWT hold the
//* free ASIDs.  GREEN this tree, RED the installed library.
//*
//GREEN    EXEC PGM=TSTASCBG,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.ASSCR
//RED      EXEC PGM=TSTASCBR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.ASSCR
//
