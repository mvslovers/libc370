#import "../bookmaster/bookmaster.typ": *

= Tasks, Synchronization and Timers <mvs-sync>

#idx("synchronization")
#idx("task", "MVS")
This chapter describes the functions with which a program runs work in
several MVS tasks, makes the tasks wait for each other, serializes access to
shared resources and measures time. They are built on the MVS services
WAIT and POST, ENQ and DEQ, ATTACH and DETACH, STIMER and TTIMER, and on the
time-of-day (TOD) clock. The headers are:

#deflist(width: 1.35in,
  [#cmd("<mvs/ecb.h>")], [event control blocks: waiting and posting
    (@mvs-sync-ecb).],
  [#cmd("<mvs/enq.h>")], [the ENQ and DEQ services (@mvs-sync-enq).],
  [#cmd("<mvs/lock.h>")], [locks on addresses and on names, built on ENQ
    (@mvs-sync-locks).],
  [#cmd("<mvs/mutex.h>")], [recursive mutexes (@mvs-sync-mutex).],
  [#cmd("<mvs/thread.h>")], [threads, that is MVS subtasks, and a pool of
    worker threads (@mvs-sync-threads, @mvs-sync-pool).],
  [#cmd("<mvs/timer.h>")], [a timer service that posts ECBs or calls
    functions after an interval (@mvs-sync-timer).],
  [#cmd("<mvs/clock.h>")], [the TOD clock and the time zone offset
    (@mvs-sync-clock).],
  [#cmd("<mvs/xmem.h>")], [address space control blocks and POST across
    address spaces (@mvs-sync-xmem).],
)

#idx("thread", "is an MVS subtask")
A _thread_ in this library is an MVS subtask: a task control block (TCB)
attached by the program, with its own C stack and its own task-level run-time
anchor, sharing the address space, the heap and the open files of the
program. There is no POSIX threads interface. The threads of a program are
dispatched by MVS independently and may run at the same time on different
processors, so data shared between them must be protected by a lock or a
mutex.

#idx("authorization", "synchronization functions")
*Authorization.* Every function in this chapter can be called by an
unauthorized program in problem state, with one exception: #cmd("__xmpost()")
uses the branch entry of POST and requires supervisor state and PSW key 0.
ENQ itself is open to every program, but MVS reserves queue names beginning
with #cmd("SYSZ") for authorized programs\; the library's own queue names do
not begin with it.

#idx("crt0", "needed for threads")
*Threads need the right start-up.* A thread is attached with
#cmd("ATTACH EP=CTHREAD"), and the name #cmd("CTHREAD") is made known to MVS
by the start-up module #cmd("crt0") when the program starts. A program
linked with the start-up module #cmd("crt1") cannot create threads:
#cmd("cthread_create()") returns #cmd("NULL"). The timer service and the
worker pool create threads of their own and have the same requirement. The
start-up modules are described in the _libc370 Programmer's Guide_.

#idx("interval", "units of 0.01 second")
*Units.* Wherever a function takes an interval named #var("bintvl"), the
unit is one hundredth of a second, as in the #cmd("BINTVL") operand of the
STIMER macro: 100 is one second.

Many functions in this chapter have an external name that differs from the
C name, such as #cmd("@@ECBWT") for #cmd("ecb_wait()")\; the header assigns
it, and only an assembler program that calls the function needs to know
it.

// -------------------------------------------------------------------------
== Event Control Blocks <mvs-sync-ecb>

#idx("ECB")
#idx("WAIT macro")
#idx("POST macro")
An event control block (ECB) is a fullword through which one task tells
another that an event has happened. A task waits on the ECB with WAIT\;
another task posts it with POST, which sets the _posted_ bit and stores a
30-bit post code in the rest of the word, and so ends the wait.

```
typedef unsigned int    ECB;

#define ECB_WAITING_BIT 0x80000000
#define ECB_POSTED_BIT  0x40000000
#define ECB_VALUE_MASK  0x3FFFFFFF
```

An ECB must be on a fullword boundary and in the address space of the task
that waits on it. WAIT returns at once for an ECB that is already posted, so
a program clears the ECB to zero before it waits on it again.
#cmd("ECB_VALUE_MASK") extracts the post code from a posted ECB.

#idx("STIMER macro", "used by timed waits")
The timed waits set a real-time interval with #cmd("STIMER REAL"), wait,
and cancel the interval with #cmd("TTIMER CANCEL"). MVS keeps one such
interval per task, so a task that uses a timed wait must not set real-time
intervals of its own with STIMER.

== ecb\_wait <mvs-sync-ecb_wait>
#idx("ecb_wait")

=== Format
```
#include <mvs/ecb.h>

int ecb_wait(ECB *ecb);
```

=== Description
Waits until #var("ecb") is posted, by issuing #cmd("WAIT ECB=").

=== Returns
0.

=== Notes
The ECB is not cleared. Read the post code from it with
#cmd("ECB_VALUE_MASK") and set it to zero before the next wait.

=== Related
@mvs-sync-ecb_post, @mvs-sync-ecb_timed_wait, @mvs-sync-cthread_wait

== ecb\_waitlist, ecb\_waitarray <mvs-sync-ecb_waitlist>
#idx("ecb_waitlist")
#idx("ecb_waitarray")

=== Format
```
#include <mvs/ecb.h>

int ecb_waitlist(ECB **ecblist);
int ecb_waitarray(ECB **ecbarray);
```

=== Description
Wait until one of several ECBs is posted.

#cmd("ecb_waitlist()") issues #cmd("WAIT ECBLIST="). #var("ecblist") is an MVS
ECB list: an array of ECB addresses in which the high-order bit is set in
the last entry.

#cmd("ecb_waitarray()") takes a dynamic array built with the array functions
of #cmd("<ext/array.h>") (see @ext), holding ECB addresses without any
marker. It copies the non-null entries, at most 256, into an ECB list, marks
the last one and waits on that list.

=== Returns
0.

=== Notes
- Both functions wait for _one_ ECB to be posted. They do not say which:
  test the posted bit of each ECB afterwards.
- #cmd("ecb_waitarray()") returns 0 at once, without waiting, when
  #var("ecbarray") is #cmd("NULL") or holds no non-null entry. Entries after
  the 256th are ignored.

=== Related
@mvs-sync-ecb_wait, @mvs-sync-ecb_timed_waitlist

== ecb\_timed\_wait <mvs-sync-ecb_timed_wait>
#idx("ecb_timed_wait")

=== Format
```
#include <mvs/ecb.h>

int ecb_timed_wait(ECB *ecb, unsigned bintvl, unsigned postcode);
```

=== Description
Waits until #var("ecb") is posted, or until #var("bintvl") hundredths of a
second have passed. When the interval expires first, the timer posts
#var("ecb") itself with the post code #var("postcode"), which ends the
wait. Choose a #var("postcode") that no other poster of the ECB uses: the
post code is the only way to tell a timeout from an event.

=== Returns
#deflist(width: 1.0in,
  [0], [The wait took place and #var("ecb") is posted, by another task or
    by the timer.],
  [negative], [The real-time interval could not be set\; the value is the
    STIMER return code, negated. No wait took place and #var("ecb") is
    unchanged.],
)

=== Notes
- #var("postcode") is reduced to 30 bits with #cmd("ECB_VALUE_MASK").
- A negative return comes from a shortage of system storage. Do not simply
  wait again: if the timer was the only poster of the ECB, a plain wait
  would never end. The first such failure in a task is reported once with a
  WTO.
- The ECB is not cleared.
- Because the timeout posts the ECB itself, do not use a timed wait on an
  ECB whose posting means something else, such as the termination ECB of a
  thread. Use #cmd("ecb_timed_waitlist()") with a separate timeout ECB.

=== Example
#code(read("../ex/mvs-sync/timedwait.c"))

=== Related
@mvs-sync-ecb_timed_waitlist, @mvs-sync-cthread_wait

== ecb\_timed\_waitlist, ecb\_timed\_waitarray <mvs-sync-ecb_timed_waitlist>
#idx("ecb_timed_waitlist")
#idx("ecb_timed_waitarray")

=== Format
```
#include <mvs/ecb.h>

int ecb_timed_waitlist(ECB **ecblist, ECB *timeecb,
                       unsigned bintvl, unsigned postcode);
int ecb_timed_waitarray(ECB **ecblist, ECB *timeecb,
                        unsigned bintvl, unsigned postcode);
```

=== Description
Wait until one of several ECBs is posted, or until #var("bintvl")
hundredths of a second have passed. #cmd("ecb_timed_waitlist()") takes an MVS
ECB list, #cmd("ecb_timed_waitarray()") a dynamic array, as described for
@mvs-sync-ecb_waitlist.

When the interval expires, #var("timeecb") is posted with #var("postcode").
#var("timeecb") should be one of the ECBs in the list, or the wait does not
end at the timeout. When #var("timeecb") is #cmd("NULL"), the first ECB of
the list is posted.

=== Returns
#deflist(width: 1.0in,
  [0], [The wait took place.],
  [negative], [The real-time interval could not be set\; the value is the
    STIMER return code, negated. No wait took place and no ECB was
    touched.],
)

#cmd("ecb_timed_waitarray()") also returns 0, without waiting, when the array
is #cmd("NULL") or holds no non-null entry.

=== Notes
See the notes for @mvs-sync-ecb_timed_wait.

=== Related
@mvs-sync-ecb_timed_wait, @mvs-sync-ecb_waitlist

== ecb\_post <mvs-sync-ecb_post>
#idx("ecb_post")

=== Format
```
#include <mvs/ecb.h>

int ecb_post(ECB *ecb, unsigned postcode);
```

=== Description
Posts #var("ecb") with the post code #var("postcode"), by issuing POST. A
task waiting on the ECB becomes ready.

=== Returns
0.

=== Notes
- The ECB must be in the address space of the caller. To post an ECB in
  another address space, use @mvs-sync-xmpost.
- POST does not return an error: an ECB at an invalid address ends the task
  with an abend.
- Pass a post code of at most 30 bits: mask it with #cmd("ECB_VALUE_MASK"),
  as #cmd("cthread_post()") does.

=== Related
@mvs-sync-ecb_wait, @mvs-sync-cthread_wait

// -------------------------------------------------------------------------
== Enqueue and Dequeue <mvs-sync-enq>

#idx("ENQ macro")
#idx("DEQ macro")
#idx("resource serialization")
The ENQ service gives a task control of a resource, the DEQ service gives it
up. A resource is identified by a queue name of up to 8 characters, a
resource name of 1 to 255 bytes and a scope:

#deflist(width: 1.35in,
  [#cmd("ENQ_STEP")], [(0, the default) the resource is known only within
    the job step: to the tasks of the address space.],
  [#cmd("ENQ_SYSTEM")], [(#cmd("0x01")) the resource is known to every
    address space of the system.],
  [#cmd("ENQ_SYSTEMS")], [(#cmd("0x02")) the resource is known to all
    systems that share it, where MVS supports that.],
)

The scope is part of the name: the same queue and resource name with
different scopes are different resources. Control is either exclusive
(#cmd("ENQ_EXC"), 0, the default) or shared (#cmd("ENQ_SHR"),
#cmd("0x04")). Any number of tasks may hold a resource shared, but only one
may hold it exclusive.

The #cmd("RET") option decides what happens when the resource is not
available:

#deflist(width: 1.35in,
  [#cmd("ENQ_HAVE")], [(0, the default) wait until it is available. A task
    that already holds the resource gets return code 8 instead of waiting.],
  [#cmd("ENQ_USE")], [(#cmd("0x40")) take it only if it is available now,
    otherwise return 4.],
  [#cmd("ENQ_TEST")], [(#cmd("0x80")) only test whether it is available\;
    nothing is acquired.],
  [#cmd("ENQ_CHNG")], [(#cmd("0x20")) change shared control already held
    into exclusive control.],
)

A resource is released by DEQ, and by MVS when the task that holds it ends.

== \_\_enqdeq, ENQ, DEQ <mvs-sync-enqdeq>
#idx("__enqdeq")
#idx("ENQ")
#idx("DEQ")

=== Format
```
#include <mvs/enq.h>

int __enqdeq(const char *qn, const char *rn, unsigned options, int deq);

#define ENQ(qn,rn,opt)  __enqdeq((qn),(rn),(opt),0)
#define DEQ(qn,rn,opt)  __enqdeq((qn),(rn),(opt),1)
```

=== Description
Issues ENQ (#var("deq") 0) or DEQ (#var("deq") not 0) for the resource with
queue name #var("qn") and resource name #var("rn"). #var("options") is the
sum of one scope, one control and one #cmd("RET") value from the lists above.
For DEQ only the scope is used.

#var("qn") is a string of up to 8 characters\; a shorter name is padded with
blanks, a longer one is cut to 8. #var("rn") is a string\; its length,
without the terminating null, is the length of the resource name, and the
characters are used as they are, without padding.

=== Returns
#cmd("ENQ") returns the return code that MVS sets for the resource:

#deflist(width: 1.0in,
  [0], [The resource was acquired, or for #cmd("ENQ_TEST") it is
    available.],
  [4], [For #cmd("ENQ_USE") and #cmd("ENQ_TEST"): the resource is not
    available.],
  [8], [The task already holds the resource.],
)

#cmd("DEQ") returns 0 when the resource was released, and a non-zero value
when the task did not hold it.

Both return 1 without issuing the service when #var("qn") or #var("rn") is
#cmd("NULL") or #var("rn") is empty.

=== Notes
- #var("rn") must be 1 to 255 bytes long. The length is stored in one byte,
  so a longer name is not refused but cut, modulo 256.
- A DEQ must use the scope of the ENQ, or it addresses a different resource
  and returns 8.
- #idx("__enq")#idx("__deq")The header also declares #cmd("__enq") and
  #cmd("__deq"). The library contains no definition of them, and a program
  that calls them cannot be linked\; use #cmd("ENQ") and #cmd("DEQ").

=== Related
@mvs-sync-locks

// -------------------------------------------------------------------------
== Locks <mvs-sync-locks>

#idx("lock", "on an address")
#idx("lock", "on a name")
The lock functions are ENQ and DEQ with fixed queue names. They serialize
tasks by the address of an object or by a name. Every function takes a
#var("read") argument: 0 (#cmd("LOCK_EXC") or #cmd("LOCK_RW")) asks for
exclusive control, 1 (#cmd("LOCK_SHR") or #cmd("LOCK_READ")) for shared
control.

#tab(caption: [Queue and resource names used by the lock functions])[
#table(columns: 4, align: left,
  table.header([*Functions*], [*Scope*], [*Queue name*], [*Resource name*]),
  [#cmd("lock()") ... #cmd("unlock()")], [step], [#cmd("CLIBLOCK")],
    [#cmd("LOCK.") and the address in 8 hexadecimal digits],
  [#cmd("syslock()") ... #cmd("sysunlock()")], [system], [#cmd("CSYSLOCK")],
    [#cmd("G.LOCK.") and the address in 8 hexadecimal digits],
  [#cmd("lock_res()") ... #cmd("unlock_resf()")], [step], [#cmd("CLIBLOCK")],
    [the name given],
  [#cmd("mtxlock()") ... #cmd("mtxunlk()")], [step], [#cmd("CLIBLOCK")],
    [#cmd("MUTEX.") and the address in 8 hexadecimal digits],
  [#cmd("cthread_lock()"), #cmd("cthread_unlock()")], [step],
    [#cmd("LCTHREAD")], [the name given],
)] <mvs-sync-locknames>

The header spells these names out as #cmd("LOCKQNAME"), #cmd("LOCKRNAME"),
#cmd("SYSLOCKQNAME"), #cmd("SYSLOCKRNAME") and #cmd("CLIB_MUTEX_RNAME").
Since #cmd("lock_res()") and #cmd("lock()") share a queue name, a name lock
whose name begins with #cmd("LOCK.") or #cmd("MUTEX.") can collide with an
address lock or a mutex.

Locks of step scope serialize the tasks of one address space. A lock is
held by the _task_, not by the program: a thread that takes a lock must
release it itself. A lock that is still held when its task ends is released
by MVS.

Each lock function except the system locks exists under two names, for
example #cmd("lock()") and #cmd("__lk"). The two names are the same function:
they have the same external name.

== lock, trylock, testlock, unlock <mvs-sync-lock>
#idx("lock")
#idx("trylock")
#idx("testlock")
#idx("unlock")

=== Format
```
#include <mvs/lock.h>

int lock(void *thing, int read);        /* also __lk()     */
int trylock(void *thing, int read);     /* also __lktry()  */
int testlock(void *thing, int read);    /* also __lktest() */
int unlock(void *thing, int read);      /* also __lkunlk() */
```

=== Description
Lock the address #var("thing") among the tasks of the address space. The
storage at the address is not touched\; the address only names the lock.

#cmd("lock()") waits until the lock is available and takes it.
#cmd("trylock()") takes it only if it is available now. #cmd("testlock()")
tests whether it could be taken, without taking it. #cmd("unlock()") releases
it\; its #var("read") argument is ignored.

=== Returns
For #cmd("lock()"), #cmd("trylock()") and #cmd("testlock()"):

#deflist(width: 1.0in,
  [0], [The lock was taken (#cmd("testlock()"): it is available).],
  [4], [#cmd("trylock()") and #cmd("testlock()"): another task holds it.],
  [8], [This task already holds it.],
)

#cmd("unlock()") returns 0 when the lock was released and a non-zero value
when the task did not hold it.

=== Notes
- The locks are not recursive. A second #cmd("lock()") by the same task
  returns 8 at once, and one #cmd("unlock()") releases the lock. A caller that
  may already hold the lock keeps the return code and unlocks only when it
  was 0:

  ```
  int rc = lock(tbl, 0);
  /* ... */
  if (rc == 0) unlock(tbl, 0);
  ```
- For a recursive lock use a mutex (@mvs-sync-mutex).

=== Related
@mvs-sync-syslock, @mvs-sync-lock_res, @mvs-sync-mutex

== syslock, systrylock, systestlock, sysunlock <mvs-sync-syslock>
#idx("syslock")
#idx("systrylock")
#idx("systestlock")
#idx("sysunlock")

=== Format
```
#include <mvs/lock.h>

int syslock(void *thing, int read);
int systrylock(void *thing, int read);
int systestlock(void *thing, int read);
int sysunlock(void *thing, int read);
```

=== Description
The same as #cmd("lock()"), #cmd("trylock()"), #cmd("testlock()") and
#cmd("unlock()"), with scope system: the lock serializes the tasks of all
address spaces.

=== Returns
As for @mvs-sync-lock.

=== Notes
Every address space has its own private storage at the same addresses, so a
system lock on a private address collides with unrelated locks of other
address spaces. Use system locks for objects in common storage, whose
address means the same thing everywhere.

=== Related
@mvs-sync-lock

== lock\_res, trylock\_res, testlock\_res, unlock\_res <mvs-sync-lock_res>
#idx("lock_res")
#idx("trylock_res")
#idx("testlock_res")
#idx("unlock_res")

=== Format
```
#include <mvs/lock.h>

int lock_res(const char *rname, int read);      /* also __lkrn()   */
int trylock_res(const char *rname, int read);   /* also __lkuntr() */
int testlock_res(const char *rname, int read);  /* also __lkrnte() */
int unlock_res(const char *rname, int read);    /* also __lkrnun() */
```

=== Description
Lock the resource named #var("rname") among the tasks of the address space.
The functions behave as #cmd("lock()"), #cmd("trylock()"), #cmd("testlock()") and
#cmd("unlock()"), with a name in place of an address.

=== Returns
As for @mvs-sync-lock. All four also return 1 when #var("rname") is
#cmd("NULL") or empty.

=== Notes
#var("rname") must be 1 to 255 characters long.

=== Related
@mvs-sync-lock_resf, @mvs-sync-lock

== lock\_resf, trylock\_resf, testlock\_resf, unlock\_resf <mvs-sync-lock_resf>
#idx("lock_resf")
#idx("trylock_resf")
#idx("testlock_resf")
#idx("unlock_resf")

=== Format
```
#include <mvs/lock.h>

int lock_resf(const char *fmt, int read, ...);     /* also __lkrnf()  */
int trylock_resf(const char *fmt, int read, ...);  /* also __lkuntf() */
int testlock_resf(const char *fmt, int read, ...); /* also __lkrtef() */
int unlock_resf(const char *fmt, int read, ...);   /* also __lkrnuf() */
```

=== Description
The same as the functions of @mvs-sync-lock_res, with the resource name
formatted from #var("fmt") and the arguments that follow #var("read"), as
#cmd("sprintf()") formats them.

=== Returns
As for @mvs-sync-lock.

=== Notes
The name is formatted into a buffer of 256 bytes whose size is not checked:
a formatted name longer than 255 characters overwrites storage.

=== Related
@mvs-sync-lock_res

// -------------------------------------------------------------------------
== Mutexes <mvs-sync-mutex>

#idx("mutex")
A mutex is an 8-byte object that serializes the tasks of the address space
and that the task holding it may lock again. Each lock adds one to a count,
each unlock subtracts one, and the mutex is free again when the count
reaches zero. The mutex is an ENQ on the queue name #cmd("CLIBLOCK") with
the address of the mutex in the resource name (@mvs-sync-locknames).

```
typedef struct clibmutx CLIBMUTX;

struct clibmutx {
    unsigned owner;     /* TCB address of the holder, 0 if free */
    unsigned count;     /* number of times locked by the holder */
};

#define CLIBMUTX_INITIALIZER  {0,0}
```

A mutex may be static, initialized with #cmd("CLIBMUTX_INITIALIZER"), or
obtained from #cmd("mtxnew()"). When a thread ends while it holds mutexes,
the library releases them.

== mtxnew, mtxinit, mtxfree <mvs-sync-mtxnew>
#idx("mtxnew")
#idx("mtxinit")
#idx("mtxfree")

=== Format
```
#include <mvs/mutex.h>

CLIBMUTX *mtxnew(void);
void      mtxinit(CLIBMUTX *mutex);
void      mtxfree(CLIBMUTX *mutex);
```

=== Description
#cmd("mtxnew()") allocates a mutex with #cmd("calloc()") and initializes it.
#cmd("mtxinit()") initializes a mutex: it sets the owner and the count to 0.
#cmd("mtxfree()") frees a mutex obtained from #cmd("mtxnew()").

=== Returns
#cmd("mtxnew()") returns the new mutex, or #cmd("NULL") when no storage is
available.

=== Notes
- Release a mutex with #cmd("mtxunlk()") before you free it. For a mutex that
  is still held, #cmd("mtxfree()") issues an #cmd("unlock()") on the address of
  the mutex, which is not the resource the mutex holds, so the ENQ remains
  held until the task ends.
- #cmd("mtxfree()") does not accept #cmd("NULL").
- Do not call #cmd("mtxinit()") for a mutex that is held.

=== Related
@mvs-sync-mtxlock

== mtxlock, mtxtry, mtxunlk <mvs-sync-mtxlock>
#idx("mtxlock")
#idx("mtxtry")
#idx("mtxunlk")

=== Format
```
#include <mvs/mutex.h>

void mtxlock(CLIBMUTX *mutex);
int  mtxtry(CLIBMUTX *mutex);
void mtxunlk(CLIBMUTX *mutex);
```

=== Description
#cmd("mtxlock()") waits until the mutex is free or held by the calling task,
then makes the calling task its holder and adds one to the count.

#cmd("mtxtry()") does the same when that is possible at once, and otherwise
returns without waiting.

#cmd("mtxunlk()") subtracts one from the count when the calling task holds the
mutex, and releases the mutex when the count reaches zero. It does nothing
when the calling task does not hold the mutex.

=== Returns
#cmd("mtxtry()") returns 0 when the calling task now holds the mutex, and 4
when another task holds it.

=== Related
@mvs-sync-mtxheld, @mvs-sync-lock

== mtxheld, mtxnheld, mtxavail <mvs-sync-mtxheld>
#idx("mtxheld")
#idx("mtxnheld")
#idx("mtxavail")

=== Format
```
#include <mvs/mutex.h>

int mtxheld(CLIBMUTX *mutex);
int mtxnheld(CLIBMUTX *mutex);
int mtxavail(CLIBMUTX *mutex);
```

=== Description
Test the state of a mutex without changing it. #cmd("mtxheld()") is true when
the calling task holds the mutex. #cmd("mtxnheld()") is true when it does not:
the mutex is free or held by another task. #cmd("mtxavail()") is true when no
task holds it.

=== Returns
1 for true, 0 for false.

=== Notes
The result of #cmd("mtxnheld()") and #cmd("mtxavail()") is only a snapshot:
another task may lock the mutex right after the test. Use #cmd("mtxtry()") to
take a mutex that is free.

=== Related
@mvs-sync-mtxlock

#note[#idx("mtxclup")The header also declares #cmd("mtxclup"). It is the
routine with which the library releases the mutexes of a thread that ends,
and is not meant to be called by a program.]

// -------------------------------------------------------------------------
== Threads <mvs-sync-threads>

#idx("thread")
#idx("ATTACH macro")
#idx("CTHDTASK")
#cmd("cthread_create()") attaches a subtask that calls a C function. The
subtask is attached with #cmd("ATTACH EP=CTHREAD,DPMOD=-1"): its dispatching
priority is one below that of the task that creates it. The function is
called with two arguments of the program's choice,

```
int func(void *arg1, void *arg2);
```

on a stack of its own, which the library allocates from the heap together
with the thread handle. The thread ends when the function returns or calls
#cmd("cthread_exit()"). Its return value is stored in the handle.

The handle, a #cmd("CTHDTASK"), remains after the thread has ended, until
#cmd("cthread_delete()") frees it. A program reads these fields of it:

#deflist(width: 1.35in,
  [#cmd("tcb")], [the TCB address of the thread, 0 when it was never
    attached or has been detached.],
  [#cmd("termecb")], [the ECB that MVS posts when the thread ends. Its
    posted bit (#cmd("0x40000000")) is the one reliable sign that the
    thread has ended\; its post code is the completion code of the task.],
  [#cmd("rc")], [the return value of the thread function, valid once
    #cmd("termecb") is posted.],
)

The task that created a thread must wait for its end, and delete it, before
the task itself ends: MVS ends a task that still has subtasks abnormally.

#idx("CTHREAD_DETACH_LIVE")
The library never detaches a thread that is still running. A forced DETACH
would end it abnormally while its stack, which is part of the handle, is
being freed. #cmd("cthread_detach()") and #cmd("cthread_delete()") therefore
refuse a thread whose #cmd("termecb") is not yet posted.

== cthread\_create, cthread\_create\_ex <mvs-sync-cthread_create>
#idx("cthread_create")
#idx("cthread_create_ex")

=== Format
```
#include <mvs/thread.h>

CTHDTASK *cthread_create(void *func, void *arg1, void *arg2);
CTHDTASK *cthread_create_ex(void *func, void *arg1, void *arg2,
                            unsigned stacksize);
```

=== Description
Create a thread that calls #var("func") with the arguments #var("arg1") and
#var("arg2"). #cmd("cthread_create()") gives the thread a stack of 64 KB,
#cmd("cthread_create_ex()") one of #var("stacksize") bytes. A
#var("stacksize") of 0 means 64 KB, a size below 80 is raised to 80, and
every other size is rounded up to a multiple of 8.

When the calling task has no handle yet, one is created for it, so that it
is recorded as the owner of the new thread.

=== Returns
The handle of the new thread, or #cmd("NULL") when the thread could not be
created: no storage, the run-time anchors are missing, or the ATTACH
failed.

=== Notes
- The ATTACH fails in a program linked with the start-up module
  #cmd("crt1"). See the introduction to this chapter.
- #var("stacksize") must be less than 1 MB (#cmd("0x100000")). Only the
  low-order 20 bits of the rounded size are kept, so a size of 1 MB or more
  gives a stack of that size modulo 1 MB, and exactly 1 MB gives a stack too
  small to run in.
- When #var("func") is #cmd("NULL"), the thread runs a library routine that
  writes the fields of its handle to the operator with WTO and returns -1.

=== Example
#code(read("../ex/mvs-sync/thread.c"))

=== Related
@mvs-sync-cthread_delete, @mvs-sync-cthread_exit, @mvs-sync-cthread_self

== cthread\_exit <mvs-sync-cthread_exit>
#idx("cthread_exit")

=== Format
```
#include <mvs/thread.h>

int cthread_exit(int rc);
```

=== Description
Ends the calling thread with the return value #var("rc"), as if its thread
function had returned #var("rc"). Before the thread ends, the functions it
pushed with #cmd("cthread_push()") are popped and called, each under
#cmd("try()"), and the mutexes it holds are released. The same happens when
the thread function returns.

=== Returns
#cmd("cthread_exit()") does not return.

=== Notes
Call it only in a thread created by #cmd("cthread_create()"). To end the main
task, use #cmd("exit()").

=== Related
@mvs-sync-cthread_push, @mvs-sync-cthread_create

== cthread\_detach <mvs-sync-cthread_detach>
#idx("cthread_detach")
#idx("DETACH macro")

=== Format
```
#include <mvs/thread.h>

int cthread_detach(CTHDTASK *task);
```

=== Description
Detaches the subtask of a thread that has ended, by issuing
#cmd("DETACH ...,STAE=YES"), and sets #cmd("tcb") in the handle to 0. The
handle itself remains. A thread that has not ended is not detached.

=== Returns
#deflist(width: 1.6in,
  [0], [The subtask was detached, or there was nothing to detach:
    #var("task") is #cmd("NULL") or its #cmd("tcb") is 0.],
  [#cmd("CTHREAD_DETACH_LIVE") (-1)], [The thread has not ended. Nothing
    was done.],
  [other], [The return code of DETACH, or the abend code when DETACH
    failed.],
)

=== Notes
The DETACH return code is stored in the #cmd("rc") field of the handle,
where it replaces the return value of the thread function. Read
#cmd("task->rc") before you detach the thread.

=== Related
@mvs-sync-cthread_delete

== cthread\_delete <mvs-sync-cthread_delete>
#idx("cthread_delete")

=== Format
```
#include <mvs/thread.h>

void cthread_delete(CTHDTASK **task);
```

=== Description
Deletes a thread that has ended: removes its handle from the table of
threads, detaches the subtask, frees the handle and its stack, and sets
#var("*task") to #cmd("NULL").

When the thread has not yet ended, nothing is deleted: a message is written
to the operator with WTO and #var("*task") is left unchanged, so that the
caller still has the handle.

Nothing is done when #var("task") or #var("*task") is #cmd("NULL"), or when
the handle does not carry the eye-catcher #cmd("CTHDTASK").

=== Notes
Wait for the end of the thread first, for example with
#cmd("ecb_wait(&task->termecb)"), and save #cmd("task->rc") if you need it.

=== Related
@mvs-sync-cthread_detach, @mvs-sync-cthread_create

== cthread\_self, cthread\_find, cthread\_get\_tcb <mvs-sync-cthread_self>
#idx("cthread_self")
#idx("cthread_find")
#idx("cthread_get_tcb")

=== Format
```
#include <mvs/thread.h>

CTHDTASK *cthread_self(void);
CTHDTASK *cthread_find(unsigned tcb);
unsigned  cthread_get_tcb(CTHDTASK *task);
```

=== Description
#cmd("cthread_find()") returns the handle of the thread whose TCB address is
#var("tcb"). #cmd("cthread_self()") returns the handle of the calling task.
#cmd("cthread_get_tcb()") returns the TCB address of the thread #var("task"),
or of the calling task when #var("task") is #cmd("NULL").

=== Returns
#cmd("cthread_find()") and #cmd("cthread_self()") return #cmd("NULL") when the
task has no handle. #cmd("cthread_get_tcb()") returns 0 for a thread that has
been detached.

=== Notes
The main task has a handle only after it has created its first thread.
Before that, #cmd("cthread_self()") returns #cmd("NULL") in the main task.

=== Related
@mvs-sync-cthread_create

== cthread\_wait, cthread\_timed\_wait, cthread\_post, cthread\_yield <mvs-sync-cthread_wait>
#idx("cthread_wait")
#idx("cthread_timed_wait")
#idx("cthread_post")
#idx("cthread_yield")

=== Format
```
#include <mvs/thread.h>

int cthread_wait(ECB *ecb);
int cthread_timed_wait(ECB *ecb, unsigned bintvl, unsigned postcode);
int cthread_post(ECB *ecb, unsigned code);
int cthread_yield(void);
```

=== Description
Convenience forms of the ECB functions that also clear the ECB.

#cmd("cthread_wait()") waits until #var("ecb") is posted, then takes the post
code and clears the ECB to zero in one atomic step (COMPARE AND SWAP), so
that a post arriving meanwhile is not lost.

#cmd("cthread_timed_wait()") waits as #cmd("ecb_timed_wait()") does, then takes
the post code and clears the ECB.

#cmd("cthread_post()") posts #var("ecb") with #var("code"), reduced to 30 bits.

#cmd("cthread_yield()") gives up the processor for one hundredth of a second,
by a timed wait on an ECB of its own.

=== Returns
#cmd("cthread_wait()") and #cmd("cthread_timed_wait()") return the post code:
the poster's code, or #var("postcode") when the interval expired.
#cmd("cthread_post()") and #cmd("cthread_yield()") return 0. The first three
return 0, without waiting or posting, when #var("ecb") is #cmd("NULL").

=== Notes
#cmd("cthread_timed_wait()") does not report a failure of the timer: when the
interval cannot be set it returns at once, with the current post code of the
ECB, which is 0 if nobody has posted it. Use #cmd("ecb_timed_wait()") where
the failure must be told apart.

=== Related
@mvs-sync-ecb_wait, @mvs-sync-ecb_timed_wait, @mvs-sync-ecb_post

== cthread\_push, cthread\_pop <mvs-sync-cthread_push>
#idx("cthread_push")
#idx("cthread_pop")
#idx("cleanup function", "of a thread")

=== Format
```
#include <mvs/thread.h>

int cthread_push(int (*func)(void*), void *arg);
int cthread_pop(CTHDPOP type);
```

=== Description
Each task has a stack of cleanup functions. #cmd("cthread_push()") pushes
#var("func") with its argument #var("arg"). #cmd("cthread_pop()") pops the
function pushed last and, depending on #var("type"):

#deflist(width: 1.6in,
  [#cmd("CTHDPOP_NOEXEC") (0)], [does not call it.],
  [#cmd("CTHDPOP_EXEC") (1)], [calls it.],
  [#cmd("CTHDPOP_TRY") (2)], [calls it under #cmd("try()"), so that an
    abend in it is caught.],
  [#cmd("CTHDPOP_ESTAE") (3)], [calls it under the library's ESTAE that
    reports an abend with WTO messages (#cmd("abendrpt()")).],
)

When a thread ends, every function still on its stack is popped and called
with #cmd("CTHDPOP_TRY").

=== Returns
#cmd("cthread_push()") returns 0, or a non-zero value when #var("func") is
#cmd("NULL") or no storage is available.

#cmd("cthread_pop()") returns the return value of the function for
#cmd("CTHDPOP_EXEC") and #cmd("CTHDPOP_ESTAE"), the return code of
#cmd("try()") (0, or the abend code) for #cmd("CTHDPOP_TRY"), and 0 for
#cmd("CTHDPOP_NOEXEC") or when the stack is empty.

=== Notes
The functions are run when a _thread_ ends. In the main task they are run
only by an explicit #cmd("cthread_pop()")\; for the end of the program use
#cmd("atexit()"). #cmd("try()") and #cmd("abendrpt()") are described in
@mvs-program.

=== Related
@mvs-sync-cthread_exit

== cthread\_lock, cthread\_unlock <mvs-sync-cthread_lock>
#idx("cthread_lock")
#idx("cthread_unlock")

=== Format
```
#include <mvs/thread.h>

int cthread_lock(int shared, const char *rname);
int cthread_unlock(const char *rname);
```

=== Description
#cmd("cthread_lock()") takes a lock on the resource name #var("rname"), shared
when #var("shared") is not 0, and waits until it is available.
#cmd("cthread_unlock()") releases it. The functions use ENQ with the queue
name #cmd("LCTHREAD") and step scope.

When #var("rname") is #cmd("NULL"), the name is
#cmd("CTHDX.")#var("asid")#cmd(".")#var("tcb"), built from the address space
identifier and the TCB address of the calling task: a lock that belongs to
the calling task.

=== Returns
As for @mvs-sync-lock.

=== Related
@mvs-sync-lock_res

== clib\_identify\_cthread <mvs-sync-clib_identify_cthread>
#idx("clib_identify_cthread")
#idx("IDENTIFY macro")

=== Format
```
#include <mvs/thread.h>

int clib_identify_cthread(void);
```

=== Description
Makes the thread driver #cmd("CTHREAD") known to MVS, by issuing
#cmd("IDENTIFY EPLOC=CTHREAD"). The start-up module #cmd("crt0") does this
when the program starts\; #cmd("clib_apf_setup()") also does it for an
authorized program (see @mvs-program).

=== Returns
The return code of IDENTIFY: 0 when the name was added.

=== Notes
IDENTIFY does not add a name a second time, so in a program started with
#cmd("crt0") the call returns a non-zero code and changes nothing.

// -------------------------------------------------------------------------
== Worker Thread Pool <mvs-sync-pool>

#idx("thread manager")
#idx("worker thread")
#idx("CTHDMGR")
#idx("CTHDWORK")
A thread manager keeps a pool of worker threads and hands them items of work
from a queue. #cmd("cthread_manager_init()") creates the manager, which runs
as a thread of its own, the dispatcher. The program adds items with
#cmd("cthread_queue_add()")\; the dispatcher gives each item to a worker that
is waiting, and starts another worker when items are waiting and the pool is
below its maximum. #cmd("cthread_manager_term()") shuts the pool down.

Every worker runs the same function, given to #cmd("cthread_manager_init()"):

```
int func(void *udata, CTHDWORK *work);
```

#var("udata") is the value given to #cmd("cthread_manager_init()"),
#var("work") is the worker's own handle. The function calls
#cmd("cthread_worker_wait()") in a loop until that returns
#cmd("CTHDWORK_POST_SHUTDOWN"), and then returns. An item of work is a
#cmd("void") pointer\; the library does not look at what it points to.

A worker may set options in #cmd("work->opt"):

#deflist(width: 1.8in,
  [#cmd("CTHDWORK_OPT_TIMER")], [also wake the worker, with
    #cmd("CTHDWORK_POST_TIMER"), when it has been waiting for a second or
    more.],
  [#cmd("CTHDWORK_OPT_NOWORK")], [give this worker no items.],
)

The dispatcher looks at the pool at least every 10 seconds, and every second
while a worker asks for timer posts.

== cthread\_manager\_init <mvs-sync-cthread_manager_init>
#idx("cthread_manager_init")

=== Format
```
#include <mvs/thread.h>

CTHDMGR *cthread_manager_init(unsigned count, void *func, void *udata,
                              unsigned stacksize);
```

=== Description
Creates a thread manager with at most #var("count") workers, each running
#var("func") with #var("udata") on a stack of #var("stacksize") bytes (0
means 64 KB). The dispatcher thread is created at once, with a stack of
32 KB, and starts a minimum number of workers: 3 when #var("count") is more
than 5, 1 when it is 4 or 5, and none otherwise. Further workers are started
when items are waiting.

=== Returns
The manager handle, or #cmd("NULL") when no storage is available or the
dispatcher thread cannot be created.

=== Example
#code(read("../ex/mvs-sync/pool.c"))

=== Related
@mvs-sync-cthread_manager_term, @mvs-sync-cthread_queue_add

== cthread\_queue\_add, cthread\_queue\_del <mvs-sync-cthread_queue_add>
#idx("cthread_queue_add")
#idx("cthread_queue_del")

=== Format
```
#include <mvs/thread.h>

int cthread_queue_add(CTHDMGR *mgr, void *data);
int cthread_queue_del(CTHDQUE **queue);
```

=== Description
#cmd("cthread_queue_add()") queues the item #var("data") and wakes the
dispatcher.

#cmd("cthread_queue_del()") removes a queue element from the queue of its
manager, frees it and sets #var("*queue") to #cmd("NULL"). The library uses
it to free the element of an item that a worker has finished\; a program
rarely needs it.

=== Returns
#cmd("cthread_queue_add()") returns 0, or -1 when no storage is available.
#cmd("cthread_queue_del()") returns 0.

=== Notes
#cmd("cthread_queue_add()") also returns 0, without queuing anything, when
#var("mgr") is #cmd("NULL") or the manager is shutting down or has stopped.

=== Related
@mvs-sync-cthread_worker_wait

== cthread\_worker\_wait <mvs-sync-cthread_worker_wait>
#idx("cthread_worker_wait")

=== Format
```
#include <mvs/thread.h>

int cthread_worker_wait(CTHDWORK *work, char **data);
```

=== Description
Called by a worker when it is ready for the next item. It frees the queue
element of the previous item, tells the dispatcher that the worker is
waiting, and waits to be posted. When the post brings an item, its pointer
is stored in #var("*data")\; otherwise #var("*data") is set to
#cmd("NULL").

=== Returns
#deflist(width: 2.1in,
  [#cmd("CTHDWORK_POST_REQUEST") (0)], [an item was given.],
  [#cmd("CTHDWORK_POST_TIMER") (1)], [a timer post, see
    #cmd("CTHDWORK_OPT_TIMER").],
  [#cmd("CTHDWORK_POST_SHUTDOWN") (2)], [the worker must end: return from
    the worker function.],
  [-1], [#var("work") is #cmd("NULL").],
)

=== Related
@mvs-sync-cthread_queue_add

== cthread\_manager\_term <mvs-sync-cthread_manager_term>
#idx("cthread_manager_term")

=== Format
```
#include <mvs/thread.h>

int cthread_manager_term(CTHDMGR **cthdmgr);
```

=== Description
Shuts the pool down. New items are no longer accepted, and the dispatcher is
asked to stop the workers once they are waiting. When the dispatcher has not
ended after (2 + #var("count")) seconds, the request becomes urgent: the
workers are posted with #cmd("CTHDWORK_POST_SHUTDOWN"), and the function
waits up to (5 + 6 #sym.times #var("count")) seconds more. #var("count") is
the maximum given to #cmd("cthread_manager_init()").

When every thread has ended, the dispatcher is deleted, items never handed
out are discarded, the manager is freed and #var("*cthdmgr") is set to
#cmd("NULL").

=== Returns
0 when the pool was shut down and freed, or when #var("cthdmgr") or
#var("*cthdmgr") is #cmd("NULL"). -1 when the dispatcher or a worker did not
end\; the storage of the manager is then kept, so that the threads still
running do not use freed storage, and a message is written to the operator.

=== Notes
A worker is stopped only between items, when it calls
#cmd("cthread_worker_wait()"). A worker function that does not come back to
#cmd("cthread_worker_wait()") keeps the pool from shutting down.

=== Related
@mvs-sync-cthread_manager_init

== cthread\_worker\_add, cthread\_worker\_del, cthread\_worker\_shutdown <mvs-sync-cthread_worker_add>
#idx("cthread_worker_add")
#idx("cthread_worker_del")
#idx("cthread_worker_shutdown")

=== Format
```
#include <mvs/thread.h>

int cthread_worker_add(CTHDMGR *mgr);
int cthread_worker_del(CTHDWORK **work);
int cthread_worker_shutdown(CTHDWORK *work);
```

=== Description
The dispatcher manages the pool with these functions\; a program normally
does not call them.

#cmd("cthread_worker_add()") creates one more worker thread.
#cmd("cthread_worker_shutdown()") posts a worker with
#cmd("CTHDWORK_POST_SHUTDOWN") and waits up to 5 seconds for it to end.
#cmd("cthread_worker_del()") removes a worker that has ended from the pool,
frees it, its thread and the queue element it still holds, and sets
#var("*work") to #cmd("NULL").

=== Returns
#cmd("cthread_worker_add()") returns 0, or -1 when #var("mgr") is
#cmd("NULL"), the manager is shutting down, or the thread cannot be created.

#cmd("cthread_worker_shutdown()") returns 0 when the worker has ended, and -1
when it is still running after 5 seconds.

#cmd("cthread_worker_del()") returns 0, or -1 when the worker's thread is still
running.

=== Notes
A worker that does not end is marked #cmd("CTHDWORK_STATE_STUCK") in
#cmd("work->state") and kept, with all its storage, rather than ended by
force.

// -------------------------------------------------------------------------
== Timer Service <mvs-sync-timer>

#idx("timer service")
#idx("TQE")
#idx("TMR")
The timer service runs functions or posts ECBs after an interval, once or
repeatedly, for any number of requests at the same time. One timer thread
per program serves all requests: it is started by the first request and
keeps a list of timer queue elements (TQEs), each identified by a
#cmd("TQEID"). An identifier is a number between 1001 and
#cmd("0xFFFFFF")\; 0 means that a request failed.

A request is one of three kinds:

#deflist(width: 1.2in,
  [one-shot], [fires once and is then removed: #cmd("tmr_ecb()"),
    #cmd("tmr_func()").],
  [kept], [fires once and is then disabled but kept, so that it can be
    enabled again: #cmd("tmr_ecb_keep()"), #cmd("tmr_func_keep()").],
  [repeating], [fires every interval until it is purged:
    #cmd("tmr_ecb_every()"), #cmd("tmr_func_every()").],
)

When a request fires, its ECB is posted with the #cmd("TQEID") as post code,
or its function is called, under #cmd("try()"), as

```
int func(void *udata, TQE *tqe);
```

The function runs on the timer thread, not on the task that made the
request. Its return value is ignored. It must return quickly, since all
other requests wait meanwhile, and must protect the data it shares with
other tasks.

The timer service needs threads, see the introduction to this chapter. The
timer thread wakes when the next request is due, and at least every 2
seconds. The resolution is a hundredth of a second, but the actual delay of
a request depends on the load of the system.

#idx("TMRSEC")
Times are #cmd("TMRSEC") values: a #cmd("double") holding seconds since
00:00 on 1 January 1970, with microseconds in the fraction.

== tmr\_ecb, tmr\_ecb\_keep, tmr\_ecb\_every <mvs-sync-tmr_ecb>
#idx("tmr_ecb")
#idx("tmr_ecb_keep")
#idx("tmr_ecb_every")

=== Format
```
#include <mvs/timer.h>

TQEID tmr_ecb(ECB *ecb, unsigned bintvl);
TQEID tmr_ecb_keep(ECB *ecb, unsigned bintvl);
TQEID tmr_ecb_every(ECB *ecb, unsigned bintvl);
```

=== Description
Request that #var("ecb") be posted after #var("bintvl") hundredths of a
second: once (#cmd("tmr_ecb()")), once and then kept disabled
(#cmd("tmr_ecb_keep()")), or every #var("bintvl") hundredths of a second
(#cmd("tmr_ecb_every()")). The timer thread is started if it is not running.

=== Returns
The identifier of the request, or 0 when it could not be made.

=== Notes
- The post code is the identifier of the request.
- The ECB is posted whether or not it was cleared since the last post\; a
  repeating request does not wait for the program to take the post.
- The ECB must stay valid as long as the request exists.

=== Related
@mvs-sync-tmr_func, @mvs-sync-tqe

== tmr\_func, tmr\_func\_keep, tmr\_func\_every <mvs-sync-tmr_func>
#idx("tmr_func")
#idx("tmr_func_keep")
#idx("tmr_func_every")

=== Format
```
#include <mvs/timer.h>

TQEID tmr_func(int (*func)(void*, TQE*), void *udata, unsigned bintvl);
TQEID tmr_func_keep(int (*func)(void*, TQE*), void *udata,
                    unsigned bintvl);
TQEID tmr_func_every(int (*func)(void*, TQE*), void *udata,
                     unsigned bintvl);
```

=== Description
Request that #var("func") be called with #var("udata") after #var("bintvl")
hundredths of a second: once, once and then kept disabled, or every
#var("bintvl") hundredths of a second. The function runs on the timer
thread.

=== Returns
The identifier of the request, or 0 when it could not be made.

=== Example
#code(read("../ex/mvs-sync/timer.c"))

=== Related
@mvs-sync-tmr_ecb, @mvs-sync-tqe

== tqe\_enable, tqe\_disable, tqe\_reset, tqe\_purge, tqe\_get <mvs-sync-tqe>
#idx("tqe_enable")
#idx("tqe_disable")
#idx("tqe_reset")
#idx("tqe_purge")
#idx("tqe_get")

=== Format
```
#include <mvs/timer.h>

int  tqe_enable(TQEID id);
int  tqe_disable(TQEID id);
int  tqe_reset(TQEID id, unsigned bintvl);
int  tqe_purge(TQEID id);
TQE *tqe_get(TQEID id);
```

=== Description
Act on the request #var("id").

#cmd("tqe_disable()") suspends it: it does not fire until it is enabled.
#cmd("tqe_enable()") enables it again and starts its interval anew, so that
it fires one interval from now.

#cmd("tqe_reset()") changes its interval to #var("bintvl") and, unless the
request is disabled, starts the interval anew.

#cmd("tqe_purge()") removes the request and frees it.

#cmd("tqe_get()") returns the TQE of the request.

=== Returns
#cmd("tqe_enable()"), #cmd("tqe_disable()"), #cmd("tqe_reset()") and
#cmd("tqe_purge()") return 0, or #cmd("ENOENT") when no request has the
identifier #var("id"). #cmd("tqe_get()") returns #cmd("NULL") in that case.

=== Notes
- A one-shot request is removed when it fires, and its identifier is then
  unknown. A kept request remains until it is purged.
- The pointer from #cmd("tqe_get()") is valid only as long as the request
  exists, and the timer thread may change the TQE meanwhile.

=== Related
@mvs-sync-tmr_ecb, @mvs-sync-tmr_func

== tmr\_start, tmr\_stop, tmr\_init, tmr\_get, tmr\_id <mvs-sync-tmr_start>
#idx("tmr_start")
#idx("tmr_stop")
#idx("tmr_init")
#idx("tmr_get")
#idx("tmr_id")

=== Format
```
#include <mvs/timer.h>

int   tmr_start(void);
int   tmr_stop(void);
int   tmr_init(void);
TMR  *tmr_get(void);
TQEID tmr_id(void);
```

=== Description
#cmd("tmr_start()") starts the timer thread if it is not running and waits for
it to begin. The request functions call it\; a program need not.

#cmd("tmr_stop()") stops the timer thread. It asks the thread to end once no
request is left, then to end at once, waits up to 5 seconds for it, and
deletes it. A program that has used the timer service calls it before it
ends: the timer thread is a subtask of the task that started it.

#cmd("tmr_init()") initializes the program's timer anchor (#cmd("TMR")), which
#cmd("tmr_get()") returns. #cmd("tmr_id()") returns a new identifier, unique
among the current requests. Programs do not normally need these three.

=== Returns
#cmd("tmr_start()") returns 0, or #cmd("ENOMEM") when the timer thread could
not be created or did not begin. #cmd("tmr_stop()") and #cmd("tmr_init()")
return 0, or -1 when the timer anchor cannot be obtained. #cmd("tmr_get()")
returns #cmd("NULL") in that case, and #cmd("tmr_id()") returns 0.

=== Notes
- #cmd("tmr_stop()") writes messages to the operator with WTO as it stops the
  thread.
- When the timer thread does not end, #cmd("tmr_stop()") keeps its handle
  rather than detach a running task.

#note[#idx("tmr_thread")#idx("tqe_new")The header also declares
#cmd("tmr_thread"), the function that the timer thread runs, and
#cmd("tqe_new"), with which the request functions build a TQE. Both are
internal to the timer service and are not meant to be called by a
program.]

== tmr\_secs <mvs-sync-tmr_secs>
#idx("tmr_secs")

=== Format
```
#include <mvs/timer.h>

TMRSEC tmr_secs(TMRSEC *secs);
```

=== Description
Reads the TOD clock and returns the time as seconds since 00:00 on 1 January
1970, with microseconds in the fraction. When #var("secs") is not
#cmd("NULL"), the value is also stored there.

=== Returns
The time in seconds.

=== Notes
The TOD clock is taken to run in Coordinated Universal Time. Leap seconds
are not counted.

=== Related
@mvs-sync-getclk

// -------------------------------------------------------------------------
== Clock and Time Zone <mvs-sync-clock>

#idx("TOD clock")
#idx("time zone offset")
The header #cmd("<mvs/clock.h>") gives direct access to the TOD clock and
to the time zone offset of the system. The functions of the standard header
#cmd("<time.h>") are built on them (see @std-time).

== \_\_getclk <mvs-sync-getclk>
#idx("__getclk")

=== Format
```
#include <mvs/clock.h>

unsigned int __getclk(void *buf);
```

=== Description
Stores the 8-byte value of the TOD clock, as STORE CLOCK gives it, into
#var("buf"), and returns the time as whole seconds since 00:00 on 1 January
1970.

=== Returns
The seconds since 1970.

=== Notes
- #var("buf") needs no alignment.
- The TOD clock is taken to run in Coordinated Universal Time.
- The seconds are computed with a signed 32-bit division and are valid up
  to the year 2038.

=== Related
@mvs-sync-tmr_secs, @mvs-sync-gettz

== \_\_gettz <mvs-sync-gettz>
#idx("__gettz")
#idx("CVTTZ")

=== Format
```
#include <mvs/clock.h>

int __gettz(void);
```

=== Description
Returns the time zone offset of the system, the field #cmd("CVTTZ") of the
CVT: local time minus Coordinated Universal Time, in units of 1.048576
seconds (bit 31 of the TOD clock).

=== Returns
The offset, negative west of Greenwich. Multiply it by 1.048576 and round to
get seconds.

=== Related
@mvs-sync-getclk

// -------------------------------------------------------------------------
== Cross-Memory Services <mvs-sync-xmem>

#idx("address space")
#idx("ASCB")
#idx("cross-memory POST")
The header #cmd("<mvs/xmem.h>") lets a program find the address space
control block (ASCB) of an address space and post an ECB that lies in
another address space. That is how a server running in its own address
space ends the wait of a client in another.

== \_\_ascb <mvs-sync-ascb>
#idx("__ascb")
#idx("ASVT")

=== Format
```
#include <mvs/xmem.h>

void *__ascb(unsigned asid);
```

=== Description
Returns the ASCB of the address space with identifier #var("asid"), taken
from the address space vector table (ASVT). When #var("asid") is 0, it
returns the ASCB of the calling address space.

=== Returns
The address of the ASCB, or #cmd("NULL") when #var("asid") is out of range
or no address space with that identifier exists.

=== Notes
- The ASCB is read without serialization. An address space may end, and its
  identifier be reused, right after the call.
- The highest identifier of the system, the value of #cmd("ASVTMAXU"), is
  treated as out of range.

=== Related
@mvs-sync-xmpost

== \_\_xmpost <mvs-sync-xmpost>
#idx("__xmpost")
#idx("CVT0PT01")

=== Format
```
#include <mvs/xmem.h>

void __xmpost(void *ascb, void *ecb, unsigned postcode);
```

=== Description
Posts the ECB at #var("ecb") in the address space whose ASCB is
#var("ascb"), with the post code #var("postcode"), through the branch entry
of POST (#cmd("CVT0PT01")) for cross-memory POST.

=== Notes
- The caller must be in supervisor state and PSW key 0, as the branch entry
  requires. #cmd("__xmpost()") does not switch state itself\; an authorized
  program gets there with #cmd("super_key_do()") (see @mvs-program).
- The post code is reduced to 30 bits.
- An error of POST is not reported. When #var("ascb") or #var("ecb") is
  #cmd("NULL"), nothing is posted and a message is written to the operator
  with WTO.
- The ECB must remain in storage of the target address space for as long
  as the post can arrive.

=== Related
@mvs-sync-ascb, @mvs-sync-ecb_post
