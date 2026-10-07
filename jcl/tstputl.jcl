//TSTPUTL  JOB (SYS),'LIBC370 463',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #463 - fopen("*PUTLINE","w") under a TMP.  See
//* test/mvs/tstputl.c.  The modules sit in the scratch PDS recvput.jcl
//* restores into.
//*
//* TMP: a batch TMP.  TSTPUT is CALLed (no CPPL) and run as a command
//* (CPPL), TSTPUTP uses __premain(), TSTPUTR is the installed library.
//* Each line of TSTPUT and TSTPUTP must appear in SYSTSPRT between the
//* READY prompts; TSTPUTR's go to a SYSOUT of their own instead.
//* NOTMP: plain batch, the open must fail with ENODEV (RC 0); RED, the
//* installed library, opens a SYSOUT instead (RC 1).
//*
//* Run:     mvsdev JOB01607, 2026-10-07: as in test/mvs/tstputl.c; RED
//*          CC 0001, the rest CC 0000.
//*
//TMP      EXEC PGM=IKJEFT01,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PUTSCR
//SYSTSPRT DD  SYSOUT=*
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//SYSTSIN  DD  *
CALL 'IBMUSER.LIBC370.PUTSCR(TSTPUT)'
TSTPUT
CALL 'IBMUSER.LIBC370.PUTSCR(TSTPUTP)'
CALL 'IBMUSER.LIBC370.PUTSCR(TSTPUTR)'
/*
//NOTMP    EXEC PGM=TSTPUT,PARM='NOTMP',REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PUTSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//RED      EXEC PGM=TSTPUTR,PARM='NOTMP',REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PUTSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
