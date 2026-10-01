/* @@INNTOA.C - inet_ntoa(): inet_ntop() into a buffer per process (#51)
**
** BSD's inet_ntoa() returns a static buffer.  A load module that may run
** reentrant cannot write its own statics, so the buffer comes from
** __wsaget(), keyed by this read-only template: one per process, which the
** next call overwrites -- the classic semantics, and as unsafe between
** threads.  inet_ntop() with a buffer of the caller's is the safe form.
*/
#include <arpa/inet.h>
#include <mvs/wsa.h>

static const char ntoa_buffer[INET_ADDRSTRLEN] = "0.0.0.0";

char *inet_ntoa(struct in_addr in)
{
    char    *buf    = __wsaget((void *) ntoa_buffer, sizeof(ntoa_buffer));

    if (!buf)
        return 0;
    return (char *) inet_ntop(AF_INET, &in, buf, sizeof(ntoa_buffer));
}
