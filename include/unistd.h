#ifndef UNISTD_H
#define UNISTD_H
/* unistd.h - the POSIX <unistd.h> functions libc370 has (D2).
**
** libc370 2.0 (#256, #250).
*/

/* sleep() - wait the given number of seconds on a timed ECB.
**           Always returns 0: nothing on this target cuts the wait short. */
int sleep(unsigned seconds);

/* usleep() - wait about usec microseconds (STIMER in units of
**            26 microseconds, at least one).  Returns 0. */
int usleep(unsigned usec);

#endif /* UNISTD_H */
