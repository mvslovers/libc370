/*
 * tstflkm.c - the module tstflock.c LINKs on a thread (#470).
 *
 * A load module with a startup of its own, so with its own PPA and a GRT
 * that has no thread table, as a module LINKed into a server's worker
 * thread has.  PARM: the hex address of the caller's FILE.  It writes one
 * line to that stream, while the caller holds the stream's lock; the
 * write has to wait for the caller's unlock().
 *
 * RC: 0 = the fputs() waited a second or more, 1 = it returned at once
 * (the lock was skipped), 2 = no usable PARM.
 */
#include <stdio.h>

static unsigned long long now(void)
{
    unsigned long long t;
    __asm__ __volatile__("STCK %0" : "=m"(t) : : "cc", "memory");
    return t;
}

int main(int argc, char **argv)
{
    unsigned            addr = 0;
    unsigned long long  t0;

    if (argc < 2 || sscanf(argv[1], "%x", &addr) != 1 || !addr) return 2;
    t0 = now();
    fputs("module line under main's lock\n", (FILE *)addr);
    return ((now() - t0) >> 12) >= 1000000ULL ? 0 : 1;
}
