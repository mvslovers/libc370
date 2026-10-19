//TSTMATHL JOB (SYS),'LIBC370 340',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #340 - the long double functions.  See test/mvs/tstmathl.c.
//* GREEN only: the RED is the build (no declarations, no link).
//*
//GREEN    EXEC PGM=TSTMTLG,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.MLSCR
//
