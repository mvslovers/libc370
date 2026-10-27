/*
 * tstquiin.c - the module that TSTQUILK (#714) LINKs.  First character of
 * PARM:
 *
 *   W  write "TSTQUIIN W WAITING" and wait on an ECB that is never posted
 *   X  write "TSTQUIIN X ABENDING" and end with S0C1
 *
 * See test/mvs/tstquilk.c for the build and the run.
 */
#include <mvs/wto.h>
#include <mvs/ecb.h>

static ECB  waitecb;

int main(int argc, char **argv)
{
    char    role = (argc > 1) ? argv[1][0] : 'W';

    wtof("TSTQUIIN %c %s", role, role == 'X' ? "ABENDING" : "WAITING");
    if (role == 'X') {
        __asm__("DC\tH'0'");
    }
    ecb_wait(&waitecb);
    return 0;
}
