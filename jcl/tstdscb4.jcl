//TSTDSCB4 JOB (SYS),'LIBC370 173',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #173 - struct dscb4 and __dscbv(). test/mvs/tstdscb4.c
//* GREEN: this tree's header; RED: origin/main's (same library).
//*
//GREEN    EXEC PGM=TSTDS4G,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.D4SCR
//RED      EXEC PGM=TSTDS4R,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.D4SCR
//
