//TSTECBTW JOB (SYS),'LIBC370 94',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #94 - ecb_timed_waitlist() must not WAIT on an ECB
//* nothing will post when STIMER REAL fails.  Timer-post legs, the
//* parked-plist fsa[0] checks, and the drained-region leg: malloc
//* until nothing fits, then a timed wait.  If STIMER fails under
//* the drain, the fixed libc returns a negative rc immediately
//* (one WTO per task); a pre-#94 libc HANGS right after the
//* "calling ecb_timed_wait" WTO - that hang is the red run, cancel
//* the job to end it (TIME= is CPU time and will not fire in a
//* WAIT).  If STIMER survives the drain (LSQA is fenced from the
//* region on a healthy system) the leg completes either way and
//* says so - outcome B.  Must be COND CODE 0000.
//*
//S1       EXEC PGM=TSTECBTW,REGION=6M,TIME=1
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
