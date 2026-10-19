//TSTDSALM JOB (SYS),'LIBC370 408',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #408 - __dsalc() SPACE= JCL form, NULL opts.  See test/mvs/tstdsalm.c.
//* GREEN is this tree; RED, where there is one, the installed library.
//*
//GREEN    EXEC PGM=TSTDSAG,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.DASCR
//RED      EXEC PGM=TSTDSAR,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.DASCR
//
