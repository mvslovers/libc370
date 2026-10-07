#import "../bookmaster/bookmaster.typ": *

= Multitasking <pg-tasks>

#idx("multitasking")#idx("thread")
A C program on MVS can do several things at once by running parts of itself
in MVS subtasks. The library calls such a subtask a _thread_: a task control
block (TCB) that the program attaches, with its own C stack, sharing the
address space, the heap and the open files of the program. There is no POSIX
threads interface\; the functions are those of #cmd("<mvs/thread.h>") and
the headers beside it. This chapter shows how to start a thread and wait for
its end, how tasks signal each other with event control blocks, how they
serialize access to shared data, how to hand work to a pool of threads, how
to have something happen after an interval, and how to end a program that
has threads.

Every function is described in the _libc370 Library Reference_, Chapter 30,
“Tasks, Synchronization and Timers”.

== Before You Start <pg-tasks-start>

#idx("CTHREAD", "thread driver")
*Nothing to do at link time.* A thread is attached under the name
#cmd("CTHREAD"), the thread driver of the library. A program that calls
#cmd("cthread_create()") links the driver from #cmd("libc.a"), and the C
start-up makes the name known to MVS before #cmd("main()") is called
(@pg-startup-variants). Any start-up of a C program will do; the timer
service and the worker pool, which create threads of their own, need
nothing more either. Only a module linked with #cmd("crtm") identifies
nothing itself and depends on the program that called it.

*Know what the threads share.* @pg-tasks-share-tab lists what belongs to the
program as a whole and what each task has for itself.

#tab(caption: [What threads share and what each has])[
  #table(columns: (1fr, 1fr),
    [*Shared by all tasks of the program*], [*One for each task*],
    [the heap: storage from #cmd("malloc()") may be passed between tasks],
    [the C stack and the automatic variables on it],
    [the open streams, including #cmd("stdout")], [#cmd("errno")],
    [the environment variables], [the state of #cmd("strtok()") and
      #cmd("rand()"), the results of #cmd("localtime()") and
      #cmd("asctime()")],
    [the #cmd("signal()") handlers], [the recovery set up with
      #cmd("try()") (@pg-errors)],
    [the writable static areas (@pg-rent-wsa) and #cmd("grtapp1") ...
      #cmd("grtapp3")], [#cmd("crtapp1"), #cmd("crtapp2")],
  )
] <pg-tasks-share-tab>

Each stream carries a lock, held for the length of every call that reads or
writes it, so a line written by one #cmd("printf()") is never split by
another task's output. The lock is taken whenever another task can reach
the stream -- the task has a subtask, or a task above it runs C code -- and
skipped otherwise, so a program costs nothing for it before its first
thread and after its last. A subtask that the program attaches itself
takes the lock as well\; this follows from the library source and has not
been verified on MVS. Everything else that two tasks change must be
serialized by the program (@pg-tasks-serial).

*Wait for every thread before the program ends.* MVS ends a task that still
has subtasks abnormally. The task that creates a thread must wait for the
thread's end and delete it before it ends itself, and so must the program as
a whole before #cmd("main()") returns.

*Each thread has its own stack, and it is not checked.* A thread gets 64 KB
unless it is created with #cmd("cthread_create_ex()") and another size.
Ask for less than 1 MB: the size is kept in 20 bits, and a request of 1 MB
or more gives a stack of that size modulo 1 MB. @pg-startup-stack describes
how to find out how much stack a function needs.

== Starting a Thread and Waiting for Its End <pg-tasks-thread>

#idx("cthread_create")#idx("termecb")
A thread runs a C function of the form

```
int func(void *arg1, void *arg2);
```

with two arguments of the program's choice. Its return value is stored in
the thread's handle, a #cmd("CTHDTASK"). To run a function in a thread:

+ Prepare the data the thread works on, in storage that outlives the thread:
  automatic variables of #cmd("main()") or a block from #cmd("malloc()").
+ Call #cmd("cthread_create()") with the function and its two arguments.
  Check the handle\; #cmd("NULL") means that no thread was created.
+ When you need the result, wait for the thread's end with
  #cmd("ecb_wait(&task->termecb)"). MVS posts #cmd("termecb") when the task
  ends.
+ Read #cmd("task->rc"), the return value of the function.
+ Call #cmd("cthread_delete(&task)"). It frees the handle and the stack and
  sets the handle to #cmd("NULL").

@pg-tasks-sum-fig divides a sum between two threads. If the second
#cmd("cthread_create()") fails, the program still waits for the first thread
before it returns.

#fig(caption: [Two threads, each summing half of a table])[
  #code(read("../ex/pg-tasks/sum.c"), numbers: true)
] <pg-tasks-sum-fig>

Some rules follow from how the handle works:

- *Read #cmd("task->rc") before you delete the thread*, and before
  #cmd("cthread_detach()"), which overwrites it with the DETACH return code.
- *The library never ends a running thread.* #cmd("cthread_delete()") of a
  thread that has not ended deletes nothing: it writes a message to the
  operator and leaves the handle as it was. Wait first, then delete.
- *Either wait will do.* #cmd("cthread_wait(&task->termecb)") clears the
  ECB it waited on, but #cmd("cthread_delete()") then finds the end of the
  thread in its TCB. (Up to libc370 2.3.0 it did not: the delete was
  refused and the step ended with abend #cmd("SA03"). With those releases,
  wait for #cmd("termecb") with #cmd("ecb_wait()").)
- *A thread ends itself* by returning from its function or by calling
  #cmd("cthread_exit()") with the return value. #cmd("cthread_exit()") is
  only for threads\; #cmd("main()") ends with #cmd("return") or
  #cmd("exit()").

#idx("cthread_push")
A thread that holds resources -- storage, an open file, a lock -- can push a
cleanup function with #cmd("cthread_push()"). When the thread ends, by
returning or through #cmd("cthread_exit()"), every function still pushed is
called, each under #cmd("try()"), so that an abend in one does not stop the
others. The mutexes a thread still holds are released as well
(@pg-tasks-mutex).

== Signalling with Event Control Blocks <pg-tasks-ecb>

#idx("ECB")#idx("cthread_wait")#idx("cthread_post")
An event control block (ECB) is a fullword through which one task tells
another that something has happened. One task waits on it\; another posts
it with a post code of up to 30 bits, which ends the wait. Declare an ECB as
#cmd("ECB") from #cmd("<mvs/ecb.h>"), set it to 0, and keep it on a
fullword boundary in storage that both tasks can reach -- the compiler
aligns a variable of type #cmd("ECB").

Two families of functions wait and post:

#deflist(width: 1.6in,
  [#cmd("cthread_wait()"), #cmd("cthread_post()")], [return the post code
    and clear the ECB in one atomic step, so that a post arriving meanwhile
    is not lost. Use them for an ECB that is posted again and again.],
  [#cmd("ecb_wait()"), #cmd("ecb_post()")], [wait and post and leave the
    ECB as it is. Use them for an ECB that is posted once, such as
    #cmd("termecb"), whose posted bit you may still want to test
    afterwards. To use such an ECB again, clear it yourself before the next
    wait.],
)

@pg-tasks-chan-fig gives a thread work through one ECB and takes the answer
back through another. #cmd("main()") posts #cmd("req") with
#cmd("REQ_WORK")\; the thread computes and posts #cmd("done")\; at the end
#cmd("main()") posts #cmd("REQ_QUIT"), and the thread returns.

#fig(caption: [A thread that serves requests through two ECBs])[
  #code(read("../ex/pg-tasks/chan.c"), numbers: true)
] <pg-tasks-chan-fig>

=== Waiting with a Time Limit <pg-tasks-timed>

#idx("ecb_timed_waitlist")#idx("timed wait")
The timed waits take an interval in hundredths of a second (500 is five
seconds). When the interval expires before anyone posts, the _timer_ posts
an ECB with a post code that you choose, and that ends the wait. The post
code is the only way to tell the time-out from a real event. Three rules
keep a timed wait safe:

- *Never let the timer post an ECB whose post means something else.* The
  end of @pg-tasks-chan-fig waits for the thread's #cmd("termecb") with a
  time limit, but it does not hand #cmd("termecb") to the timer: it builds
  a list of two ECBs, #cmd("termecb") and an ECB of its own,
  #cmd("tmo"), and lets #cmd("ecb_timed_waitlist()") post #cmd("tmo"). Then
  it tests the posted bit of #cmd("termecb") to see which one ended the
  wait. In an ECB list, the high-order bit marks the last entry.
- *Check for a negative return.* #cmd("ecb_timed_wait()") and
  #cmd("ecb_timed_waitlist()") return a negative value when the interval
  could not be set, and then they have not waited at all. If the timer was
  the only poster of the ECB, a plain wait in its place would never end.
  #cmd("cthread_timed_wait()") does not report this failure\; use it only
  where a wait that returns at once does no harm.
- *Leave the task's interval timer alone.* MVS keeps one real-time interval
  for each task, and the timed waits use it. A task that uses timed waits
  must not set intervals of its own with #cmd("STIMER").

== Serializing Access to Shared Data <pg-tasks-serial>

#idx("serialization")#idx("lock")#idx("mutex")
When two tasks change the same data, one of them must wait while the other
does. The library offers four tools, from the smallest to the widest:

#tab(caption: [Ways to serialize])[
  #table(columns: (1.5in, 1.25in, 1fr), align: left,
    [*Tool*], [*Reaches*], [*Use it for*],
    [#cmd("__inc()"), #cmd("__cas()") \ in #cmd("<s370/atomic.h>")], [one
      fullword], [counters and flags: one #cmd("COMPARE AND SWAP"), no
      waiting],
    [#cmd("lock()"), #cmd("lock_res()") \ in #cmd("<mvs/lock.h>")], [the
      tasks of the address space], [a resource named by an address or a
      name],
    [#cmd("mtxlock()") \ in #cmd("<mvs/mutex.h>")], [the tasks of the
      address space], [a resource that a function may lock again while it
      holds it],
    [#cmd("ENQ") \ in #cmd("<mvs/enq.h>")], [any address space, with
      #cmd("ENQ_SYSTEM")], [a resource shared with other programs: a data
      set, a member, a record],
  )
] <pg-tasks-serial-tab>

All but the first are built on the MVS service ENQ. A lock or an ENQ belongs
to the _task_ that took it: a thread that takes one releases it itself, and
MVS releases what a task still holds when the task ends.

=== Locks <pg-tasks-locks>

#idx("lock_res")
#cmd("lock(")#var("addr")#cmd(", LOCK_EXC)") locks an address among the
tasks of the address space\; the storage at the address is not touched, the
address only names the lock. #cmd("lock_res()") does the same with a name of
up to 255 characters. A second argument of #cmd("LOCK_SHR") in place of
#cmd("LOCK_EXC") asks for shared control, which any number of readers may
hold at once.

*Locks are not recursive.* A task that asks again for a lock it holds gets
return code 8 at once, and one unlock releases the lock. A function that may
be called with the lock already held keeps the return code and unlocks only
when it took the lock itself, as #cmd("update_account()") in
@pg-tasks-reslock-fig does.

#fig(caption: [A lock on a name, and an ENQ shared with other address spaces])[
  #code(read("../ex/pg-tasks/reslock.c"), numbers: true)
] <pg-tasks-reslock-fig>

To lock a name that you build from data, format it into a buffer of your own
with #cmd("snprintf()") and pass it to #cmd("lock_res()"), as the example
does. The #cmd("lock_resf()") family formats the name itself, but into a
fixed buffer of 256 bytes without a length check, so a long name overwrites
storage.

The names of the locks share the queue name #cmd("CLIBLOCK") with the
address locks and the mutexes. Do not begin a name with #cmd("LOCK.") or
#cmd("MUTEX."), or it may collide with them.

=== Mutexes <pg-tasks-mutex>

#idx("mtxnew")#idx("mtxlock")
A mutex is a lock that the task holding it may take again: each
#cmd("mtxlock()") adds one to a count, each #cmd("mtxunlk()") subtracts
one, and the mutex is free when the count is zero. That makes it the tool
for a data structure whose functions call each other, as
#cmd("tbl_pair()") calls #cmd("tbl_add()") in @pg-tasks-table-fig.

#fig(caption: [A table protected by a mutex])[
  #code(read("../ex/pg-tasks/table.c"), numbers: true)
] <pg-tasks-table-fig>

Obtain the mutex with #cmd("mtxnew()"), as the example does. A mutex is
eight bytes that #cmd("mtxlock()") changes, so a mutex declared
#cmd("static") with #cmd("CLIBMUTX_INITIALIZER") would be static storage
that the program stores into -- which a reentrant program, or one in the
link list, must not have (@pg-rent).

Release a mutex completely with #cmd("mtxunlk()") before you free it with
#cmd("mtxfree()"). #cmd("mtxfree()") of a mutex that is still held does not
release the ENQ behind it, which then stays held until the task ends.

=== ENQ Across Address Spaces <pg-tasks-enq>

#idx("ENQ")#idx("DEQ")
The locks and mutexes serialize the tasks of one program. To serialize with
other jobs, use #cmd("ENQ") with the scope #cmd("ENQ_SYSTEM"), and a queue
name and resource name that the other programs use too, as
#cmd("try_master()") in @pg-tasks-reslock-fig does. Keep these points in
mind:

- The scope is part of the name. #cmd("DEQ") must give the scope of the
  #cmd("ENQ"), or it addresses another resource.
- #cmd("ENQ_USE") returns 4 at once when the resource is taken\; without it,
  #cmd("ENQ") waits.
- Queue names beginning with #cmd("SYSZ") are reserved for authorized
  programs. Choose a queue name of your own.
- The system locks #cmd("syslock()") ... #cmd("sysunlock()") lock an
  _address_ system-wide. Every address space has its own private storage at
  the same addresses, so use them only for objects in common storage. For
  anything else, use #cmd("ENQ") with a name.
- Use #cmd("ENQ") and #cmd("DEQ"), not #cmd("__enq") and #cmd("__deq"): the
  header declares the latter, but the library does not contain them.

=== Atomic Updates <pg-tasks-atomic>

#idx("__uinc")#idx("atomic update")
A counter or a flag that several tasks change needs no lock at all.
#cmd("__inc()"), #cmd("__dec()"), #cmd("__uinc()") and #cmd("__udec()")
change a fullword with one #cmd("COMPARE AND SWAP") instruction and return
the value it had before\; #cmd("__cas()") replaces a word only if it still
holds the value you expect. The word must be on a fullword boundary. The
pool in @pg-tasks-pool-fig and the timer in @pg-tasks-timer-fig count this
way.

== A Pool of Worker Threads <pg-tasks-pool>

#idx("thread manager")#idx("worker thread")
A program that receives many small items of work -- a server answering
requests, for example -- does better with a pool of threads that wait for
work than with a thread created for each item. The thread manager keeps such
a pool:

+ Create the manager with #cmd("cthread_manager_init()"), giving the
  largest number of workers, the worker function, a pointer the workers
  receive, and the stack size of a worker (0 for 64 KB).
+ Write the worker function as a loop around #cmd("cthread_worker_wait()").
  It returns #cmd("CTHDWORK_POST_REQUEST") with the next item,
  #cmd("CTHDWORK_POST_TIMER") without one, and
  #cmd("CTHDWORK_POST_SHUTDOWN") when the worker must return.
+ Queue items with #cmd("cthread_queue_add()"). An item is a pointer\; the
  library does not look at what it points to.
+ When all the work is done, call #cmd("cthread_manager_term()").

#fig(caption: [A pool of workers, and waiting until every item is done])[
  #code(read("../ex/pg-tasks/pool.c"), numbers: true)
] <pg-tasks-pool-fig>

*#cmd("cthread_manager_term()") does not finish the queue.* It stops the
workers as soon as they come back for more work, and discards the items that
no worker has taken yet. A program that needs every item done must find out
for itself when they are: @pg-tasks-pool-fig counts the finished items with
#cmd("__uinc()"), and the worker that finishes the last one posts
#cmd("alldone"), on which #cmd("main()") waits before it shuts the pool
down.

A worker is stopped only between items. A worker function that does not
come back to #cmd("cthread_worker_wait()") keeps the pool from shutting
down\; #cmd("cthread_manager_term()") then gives up after a while, returns
-1, and keeps the storage of the pool rather than free storage that a
running thread still uses.

== Doing Something After an Interval <pg-tasks-timer>

#idx("timer service")#idx("tmr_func_every")
The timer service runs functions or posts ECBs after an interval, once or
repeatedly. One timer thread serves all requests of the program\; it is
started by the first request.

#deflist(width: 1.6in,
  [#cmd("tmr_ecb()")], [posts an ECB once, after the interval.],
  [#cmd("tmr_ecb_every()")], [posts an ECB every interval.],
  [#cmd("tmr_func()")], [calls a function once, after the interval.],
  [#cmd("tmr_func_every()")], [calls a function every interval.],
)

Each returns an identifier, a #cmd("TQEID"), or 0 when the request could not
be made. #cmd("tqe_purge()") removes a request that is no longer wanted, and
#cmd("tmr_stop()") stops the timer thread.

#fig(caption: [A function called every second, and an ECB posted after 5.5 seconds])[
  #code(read("../ex/pg-tasks/timer.c"), numbers: true)
] <pg-tasks-timer-fig>

Three points need care:

- *A timer function runs on the timer thread*, not on the task that made
  the request. It must protect what it shares with other tasks -- the
  example counts with #cmd("__uinc()") -- and it must return quickly,
  because every other request waits meanwhile. It receives its data through
  #cmd("udata"), here a structure in #cmd("main()")\; keep the data where it
  stays valid as long as the request exists, and not in static storage.
- *An ECB that the timer posts must stay valid* as long as the request
  exists. Purge the request before the ECB goes out of scope.
- *Call #cmd("tmr_stop()") before the program ends.* The timer thread is a
  subtask of the task that started it, and a task that ends with a subtask
  still running ends abnormally.

The resolution of the intervals is a hundredth of a second, but how late a
request actually fires depends on the load of the system.

== Ending a Program That Has Threads <pg-tasks-end>

Before #cmd("main()") returns, or a thread ends that has created threads of
its own:

+ Tell each thread to end, in whatever way the program has agreed with it --
  a post code, as in @pg-tasks-chan-fig, or a flag.
+ Wait for each thread's #cmd("termecb"), with a time limit if a thread may
  hang (@pg-tasks-timed).
+ Delete each thread with #cmd("cthread_delete()").
+ Shut down a worker pool with #cmd("cthread_manager_term()") and the timer
  service with #cmd("tmr_stop()").
+ Release the locks and mutexes the task holds, and free the storage it
  obtained.

MVS releases the ENQs of a task that ends, and the library releases the
mutexes of a thread that ends, but neither does anything for the data the
lock protected: a thread that ends in the middle of an update leaves the
update half done. Use #cmd("cthread_push()") for the cleanup that must
happen however the thread ends.
