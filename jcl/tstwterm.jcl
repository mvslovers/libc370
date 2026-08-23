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
//* MEASURED on mvsdev 2026-08-23, same source both sides:
//*   PRE-FIX  "the worker returned of its own accord *** FAIL",
//*            COND CODE 0008, elapsed 13.31s.  The job log shows
//*            the handler finishing 3s before the job ends with
//*            nothing in between - the worker on the ENQ.
//*   POST-FIX 7/7 ok, COND CODE 0000, elapsed 10.21s, with
//*            "finished the request" and "returning normally"
//*            in the same second.
//*
//* No S33E message appears either way: it is real pre-fix, but
//* libc370's recovery exit is installed only via try()/estae()
//* and this worker is a bare loop.  httpd#122 was loud because
//* httpd runs handlers under try().  See test/mvs/tstwterm.c.
//*
//* TIME=1 is the watchdog - a regression would hang.
//*
//S1       EXEC PGM=TSTWTERM,REGION=4M,TIME=1
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
