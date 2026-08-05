//TSTJESLG JOB (SYS),'LIBC370 4 PROBE',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 issue #4 - jesprint() returns ZERO lines for the SYSLOG spool
//* datasets.  TSTJESLG walks the same spool block chain jesprint() walks and
//* reports every decision instead of breaking silently.  See
//* test/mvs/tstjeslg.c and doc/jes-syslog-issue4.md.
//*
//* Build:   cc370 -Iinclude test/mvs/tstjeslg.c -o tstjeslg.xmit
//* Install: RECEIVE the XMIT into the STEPLIB below.
//*
//* The three steps are ONE experiment - read them together:
//*   S1 SYSLOG  the broken case
//*   S2 HTTPD   control A: an ACTIVE STC whose SYSOUT jesprint() prints today
//*              -> if S2 works and S1 does not, "job is still executing /
//*                 checkpointed IOT is stale" is NOT the cause
//*   S3 <batch> control B: a finished batch job, fully checkpointed
//*              -> the known-good baseline for the block header layout
//*
//* Adjust: STEPLIB DSN, the HASPCKPT/HASPACE1 VOL=SER, and the S3 job name.
//*
//* PARM ',PRINT' additionally drives the real jesprint() and prints its
//* JESPRST (stop reason, blocks, lines) beside what the probe reconstructs
//* on its own - that is the red/green case for #21/#22:
//*   a purged data set  -> reason=FOREIGN, blocks=0, lines=0
//*   a held generation  -> reason=END,     lines=n
//*   an OPEN data set   -> reason=OPENEND, blocks>0 (normal end, not a loss)
//*
//S1       EXEC PGM=TSTJESLG,REGION=4M,PARM='SYSLOG,PRINT'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//HASPCKPT DD  DISP=SHR,DSN=SYS1.HASPCKPT,UNIT=3350,VOL=SER=MVS000
//HASPACE1 DD  DISP=SHR,DSN=SYS1.HASPACE,UNIT=3350,VOL=SER=SPOOL1
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//S2       EXEC PGM=TSTJESLG,REGION=4M,PARM='HTTPD,PRINT'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//HASPCKPT DD  DISP=SHR,DSN=SYS1.HASPCKPT,UNIT=3350,VOL=SER=MVS000
//HASPACE1 DD  DISP=SHR,DSN=SYS1.HASPACE,UNIT=3350,VOL=SER=SPOOL1
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//S3       EXEC PGM=TSTJESLG,REGION=4M,PARM='MBTDEPL'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//HASPCKPT DD  DISP=SHR,DSN=SYS1.HASPCKPT,UNIT=3350,VOL=SER=MVS000
//HASPACE1 DD  DISP=SHR,DSN=SYS1.HASPACE,UNIT=3350,VOL=SER=SPOOL1
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
