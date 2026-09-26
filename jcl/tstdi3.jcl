//TSTDI3   JOB (SYS),'LIBC370 187',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #187 - the long long helpers cc370 calls (@@MULDI3,
//* @@DIVDI3, @@MODDI3, @@UDIVDI, @@UMODDI, @@NEGDI2) and (u)intptr_t.
//*
//* test/host/tstdi3.c checks the arithmetic on the host.  This job checks
//* what only the target can: the S/370 code the helpers compile to, their
//* big-endian word order, and the S0C9 on a zero divisor.
//* See test/mvs/tstdi3.c.
//*
//* STEPLIB is the scratch PDS recvdi3.jcl restores into.
//*
//* Run:     mvsdev JOB00449 and JOB00453, CC 0000, 1138/1138, 2026-09-26.
//*          Proven red at 787 of 1138 with the helpers' word order
//*          swapped, JOB00451.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//RUN      EXEC PGM=TSTDI3,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.DI3SCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
