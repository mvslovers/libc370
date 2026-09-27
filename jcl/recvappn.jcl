//TSTRCVAP JOB (SYS),'RECV TSTAPPND',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* RECEIVE the #189 stdio probes (tstappnd, tstwrpos) into a scratch PDS.
//* RECEIVE cannot merge into an existing PDS, so the old copy goes first;
//* tstappnd.jcl and tstwrpos.jcl STEPLIB straight at the result.  Pack
//* every probe the next run needs into the one xmit, e.g.
//*     ld370 --pack TSTWRPOS=TSTWRPOS.iebcopy -o probe -xmit \
//*           --dsn IBMUSER.LIBC370.T189SCR
//*
//* The staging data set is this probe's own, IBMUSER.LIBC370.T189XMIT,
//* not IBMUSER.MBT.XMIT.IN, which every project's `make deploy` shares.
//* Build as the probe's header says, then upload the xmit there
//* (FB 80, created first if it is gone - RECEIVE may delete it).
//*
//DEL      EXEC PGM=IEFBR14
//D1       DD  DSN=IBMUSER.LIBC370.T189SCR,DISP=(MOD,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1))
//*
//RECV     EXEC PGM=IKJEFT01,DYNAMNBR=20,REGION=6144K
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  RECEIVE INDSN('IBMUSER.LIBC370.T189XMIT')
/*
//
