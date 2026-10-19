//TSTSENVM JOB (SYS),'LIBC370 475',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #475 - setenv() by the trimmed name.  test/mvs/tstsenvm.c
//* GREEN is this tree; RED, where there is one, the installed library.
//*
//GREEN    EXEC PGM=TSTSENVG,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SESCR
//SYSENV   DD  *
TSTA = 1
TSTA = 2
/*
//RED      EXEC PGM=TSTSENVR,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SESCR
//SYSENV   DD  *
TSTA = 1
TSTA = 2
/*
//
