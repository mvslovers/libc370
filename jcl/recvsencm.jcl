//TSTRCVSC JOB (SYS),'RECV TSTSENCM',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* RECEIVE the #475 probe into a scratch PDS.  RECEIVE cannot merge,
//* so the old copy goes first.  Staging: IBMUSER.LIBC370.SCXMIT, this
//* probe's own.  Build and pack as test/mvs/tstsencm.c says.
//*
//DEL      EXEC PGM=IEFBR14
//D1       DD  DSN=IBMUSER.LIBC370.SCSCR,DISP=(MOD,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1))
//*
//RECV     EXEC PGM=IKJEFT01,DYNAMNBR=20,REGION=6144K
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  RECEIVE INDSN('IBMUSER.LIBC370.SCXMIT')
/*
//
