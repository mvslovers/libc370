//DSNMRCVR JOB (SYS),'RECV TSTDSNMT',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* RECEIVE the #181 probe into a scratch PDS.  RECEIVE cannot merge
//* into an existing PDS, so the old copy goes first; tstdsnmt.jcl
//* STEPLIBs straight at the result.
//*
//* The staging data set is this probe's own, IBMUSER.LIBC370.DSNMTXMI,
//* not IBMUSER.MBT.XMIT.IN (every project's make deploy stages there).
//*
//* Build and pack as test/mvs/tstdsnmt.c says, then upload the xmit
//* to IBMUSER.LIBC370.DSNMTXMI (FB 80, created first if it is gone -
//* RECEIVE may delete it).
//*
//DEL      EXEC PGM=IEFBR14
//D1       DD  DSN=IBMUSER.LIBC370.DSNMTSCR,DISP=(MOD,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1))
//*
//RECV     EXEC PGM=IKJEFT01,DYNAMNBR=20,REGION=6144K
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  RECEIVE INDSN('IBMUSER.LIBC370.DSNMTXMI')
/*
//
