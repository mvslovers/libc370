//TSTRCVSL JOB (SYS),'RECV TST75SLT',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* RECEIVE the issue #162 slot-exhaustion probe into its own
//* scratch PDS.  The run step STEPLIBs there directly; no IEBCOPY
//* into a shared LINKLIB - this is a throwaway container.  The
//* target data set name is the one ld370 --pack --dsn embedded in
//* the XMIT (IBMUSER.LIBC370.SLSCR).
//*
//DEL      EXEC PGM=IEFBR14
//D1       DD  DSN=IBMUSER.LIBC370.SLSCR,DISP=(MOD,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1))
//*
//RECV     EXEC PGM=IKJEFT01,DYNAMNBR=20,REGION=6144K
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  RECEIVE INDSN('IBMUSER.LIBC370.SLXMIT')
/*
//
