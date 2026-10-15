//TSTRCV42 JOB (SYS),'RECV TSTS42',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* RECEIVE the #538 probe into a scratch PDS.  RECEIVE cannot merge,
//* so the old copy goes first.  Staging: IBMUSER.LIBC370.S42XMIT, this
//* probe's own (not IBMUSER.MBT.XMIT.IN, which every make deploy uses).
//* Build and pack as test/mvs/tsts42.c says.
//*
//DEL      EXEC PGM=IEFBR14
//D1       DD  DSN=IBMUSER.LIBC370.S42SCR,DISP=(MOD,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1))
//*
//RECV     EXEC PGM=IKJEFT01,DYNAMNBR=20,REGION=6144K
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  RECEIVE INDSN('IBMUSER.LIBC370.S42XMIT')
/*
//
