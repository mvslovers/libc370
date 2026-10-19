//TSTRCVBN JOB (SYS),'RECV TST75BND',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* RECEIVE the issue #164 SELECT-bound probe into its own scratch
//* PDS.  The run step STEPLIBs there directly.  The target data
//* set name is the one ld370 --pack --dsn embedded in the XMIT
//* (IBMUSER.LIBC370.BNSCR).
//*
//DEL      EXEC PGM=IEFBR14
//D1       DD  DSN=IBMUSER.LIBC370.BNSCR,DISP=(MOD,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1))
//*
//RECV     EXEC PGM=IKJEFT01,DYNAMNBR=20,REGION=6144K
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  RECEIVE INDSN('IBMUSER.LIBC370.BNXMIT')
/*
//
