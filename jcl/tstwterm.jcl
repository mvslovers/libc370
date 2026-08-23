//TSTWTERM JOB (SYS),'LIBC370 11',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #11 - tearing a thread manager down while a worker is
//* BUSY used to end in S33E.  Not a wedged handler: the worker
//* blocks on the manager's own ENQ, because dispatch_thread_term()
//* held it across the wait while cthread_worker_wait() needs it to
//* release the dispatched queue item before it can ever see the
//* shutdown post.  So the worker cannot reach the wait, misses the
//* 5s window, and was force-DETACHed - after which its CTHDTASK,
//* which CONTAINS its stack, was freed under it.
//*
//* The handler here is healthy and merely slow (8s), which is the
//* whole point: nothing is wedged, and it still failed.
//*
//* PRE-FIX : ABEND S33E in this log, an SVC dump, and
//*           "the worker returned of its own accord  *** FAIL",
//*           COND CODE 0008.
//* POST-FIX: no S33E, no dump, COND CODE 0000.
//*
//* TIME=1 is the watchdog - the run is ~8s clean, ~11s to the
//* pre-fix force-DETACH, and a regression would hang instead.
//* See test/mvs/tstwterm.c.
//*
//S1       EXEC PGM=TSTWTERM,REGION=4M,TIME=1
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
