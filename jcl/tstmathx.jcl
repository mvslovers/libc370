//TSTMATHX JOB (SYS),'LIBC370 389 X',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #389 - exp over the HFP range, pow, atan2, sinh/cosh/tanh.
//* See test/mvs/tstmathx.c.  GREEN is this tree, RED the installed
//* library.  The result is on the console: TSTMATHX PASS / FAIL / FIND.
//*
//GREEN    EXEC PGM=TSTMATXG,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.MXSCR
//RED      EXEC PGM=TSTMATXR,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.MXSCR
//
