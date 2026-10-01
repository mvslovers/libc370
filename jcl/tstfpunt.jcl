//TSTFPUNT JOB (SYS),'LIBC370 172 UNIT',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #172 - UNIT= and VOLSER= through fopen(), measured.
//*
//* The host test (test/host/tstfpunit.c) pins that the DALUNIT and
//* DALVLSER text units are built, and only when asked.  This job answers
//* what it cannot: whether the data set then lands on that volume.  Each
//* case creates through fopen(), writes one record, and reads the volume
//* back from the catalog and from that volume's VTOC.
//*
//* PARM is the target volume (default PUB001): mounted, reachable through
//* UNIT=SYSDA, and not where the system default puts a new data set - the
//* probe checks the last and says COULD NOT MEASURE if it is.
//*
//* The work data set IBMUSER.TSTFPUNT.WORK must NOT exist when the job
//* starts; every case creates and deletes it.
//*
//* Case (5) names a volume that does not exist and must come back
//* refused at once.  An IEF238D in the job log fails it, whatever the
//* probe printed - the probe cannot see the operator wait from inside.
//*
//* STEPLIB is the scratch PDS the RECEIVE in recvfpun.jcl restores into.
//*
//S1       EXEC PGM=TSTFPUNT,REGION=4096K,PARM='PUB001'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.UNITSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//
