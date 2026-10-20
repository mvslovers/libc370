//TRDY JOB (SYS),'RECV TSTDYNIT',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* RECEIVE the #52 probe into a scratch PDS.  RECEIVE cannot merge,
//* so the old copy goes first.  Staging: IBMUSER.LIBC370.DYXMIT, this
//* probe's own.  Build and pack as test/mvs/tstdynit.c says.
//*
//DEL      EXEC PGM=IEFBR14
//D1       DD  DSN=IBMUSER.LIBC370.DYSCR,DISP=(MOD,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1))
//*
//RECV     EXEC PGM=IKJEFT01,DYNAMNBR=20,REGION=6144K
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  RECEIVE INDSN('IBMUSER.LIBC370.DYXMIT')
/*
//
