//TSTDBLRB JOB (SYS),'LIBC370 222',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #222 - printf of a large double, a large precision or a
//* large width wrote past the conversion buffer.  See
//* test/mvs/tstdblrb.c.
//*
//* STEPLIB is the scratch PDS recvdbrb.jcl restores into.
//*
//* Run:     mvsdev JOB00722, CC 0000, 59/59, 2026-09-29.  Against the
//*          pre-fix libc: JOB00724, ABEND S0C4, PSW address 00F0F0F6.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//RUN      EXEC PGM=TSTDBLRB,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.DBRBSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
