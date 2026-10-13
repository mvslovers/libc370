//TSTRCVPX JOB (SYS),'RECV TSTPDX37',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* RECEIVE the #524 probes (TSTPDX37, TSTPDX3R) into a scratch PDS.
//* RECEIVE cannot merge into an existing PDS, so the old copy goes
//* first; tstpdx37.jcl STEPLIBs straight at the result.
//*
//* The staging data set is this probe's own, IBMUSER.LIBC370.PXXMIT,
//* not IBMUSER.MBT.XMIT.IN.  Build and pack as test/mvs/tstpdx37.c
//* says, upload tstpdx37.xmit there (FB 80, created first if it is
//* gone - RECEIVE may delete it).
//*
//DEL      EXEC PGM=IEFBR14
//D1       DD  DSN=IBMUSER.LIBC370.PXSCR,DISP=(MOD,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1))
//*
//RECV     EXEC PGM=IKJEFT01,DYNAMNBR=20,REGION=6144K
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  RECEIVE INDSN('IBMUSER.LIBC370.PXXMIT')
/*
//
