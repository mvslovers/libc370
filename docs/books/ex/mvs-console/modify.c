#include <string.h>
#include <mvs/console.h>
#include <mvs/ecb.h>
#include <mvs/wto.h>

int main(void)
{
    COM  *com = __gtcom();
    CIB  *cib;
    char text[80];
    int  stop = 0;

    if (!com) return 8;

    /* a started task begins with a START CIB: discard it */
    cib = __cibget();
    if (cib && cib->cibverb == CIBSTART) __cibdel(cib);

    __cibset(1);                    /* one MODIFY queued at a time */
    wtof("DEMO001I READY FOR COMMANDS");

    while (!stop) {
        ecb_wait(com->comecbpt);    /* posted on MODIFY and STOP */
        while ((cib = __cibget()) != NULL) {
            if (cib->cibverb == CIBSTOP) {
                stop = 1;
            } else if (cib->cibverb == CIBMODFY) {
                unsigned n = cib->cibdatln < sizeof(text) - 1
                           ? cib->cibdatln : sizeof(text) - 1;
                memcpy(text, cib->cibdata, n);
                text[n] = 0;
                wtof("DEMO002I COMMAND '%s' ACCEPTED", text);
            }
            __cibdel(cib);          /* also resets the ECB */
        }
    }
    wtof("DEMO003I ENDED");
    return 0;
}
