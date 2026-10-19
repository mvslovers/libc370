//TST75SEL JOB (SYS),'LIBC370 144',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #144 - does select() over several sockets report every
//* socket that is ready?  Measures the Hercules X'75' emulation, not
//* libc370.  Phase A (control): pairs over an own loopback listener,
//* slots and host descriptors move in step, nothing may show.  Phase
//* B: the guest connects to a port Hercules serves itself (8888 or
//* 3270), so Hercules holds a descriptor without a slot, and a low
//* socket number ends up on a high descriptor.
//*
//* See test/mvs/tst75sel.c.
//*
//*   RC=0  no anomaly, phase B layout created at least once
//*   RC=4  a ready socket missed or a false ready - defect reproduced
//*   RC=8  setup failure or layout never created - no verdict
//*
//* Results are WTOs in the job log: TST75SEL A/B/RESULT lines.
//*
//S1       EXEC PGM=TST75SEL,REGION=4096K,TIME=(0,30)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SLSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//
