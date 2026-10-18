//TSTRCVAR JOB (SYS),'RECV TSTARADM',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* RECEIVE the arrayaddf probe into a scratch PDS.  RECEIVE cannot merge,
//* so the old copy goes first.  Staging: IBMUSER.LIBC370.ARXMIT, this
//* probe's own, not IBMUSER.MBT.XMIT.IN (every make deploy uses it).
//* Build and pack as test/mvs/tstaradm.c says.
//*
//DEL      EXEC PGM=IEFBR14
//D1       DD  DSN=IBMUSER.LIBC370.ARSCR,DISP=(MOD,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1))
//*
//RECV     EXEC PGM=IKJEFT01,DYNAMNBR=20,REGION=6144K
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  RECEIVE INDSN('IBMUSER.LIBC370.ARXMIT')
/*
//
