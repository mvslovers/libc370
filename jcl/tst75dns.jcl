//TST75DNS JOB (SYS),'LIBC370 163',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #163 - does a name lookup through X'75' stop the emulated
//* CPU?  Measures the Hercules X'75' emulation, not libc370.  An
//* observer subtask notes the largest gap between its 0.01 s ticks
//* while the main task runs gethostbyname()/gethostbyaddr() on names
//* that take the host resolver seconds.  See test/mvs/tst75dns.c.
//*
//*   RC=0  slow lookups ran and the observer kept ticking: no block
//*   RC=4  the observer stalled for the lookup: the CPU blocks
//*   RC=8  no slow lookup could be produced, or setup failed
//*
//* The lookup times are the HOST resolver's (the machine Hercules
//* runs on); a container result does not carry over to a stand.
//* Run it on a system nobody else uses: a long block can trip the
//* Hercules watchdog.  Results: TST75DNS lines in the job log.
//*
//S1       EXEC PGM=TST75DNS,REGION=4096K,TIME=(0,30)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.DNSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//
