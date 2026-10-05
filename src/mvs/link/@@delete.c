#include <stddef.h>
#include <ibm/mvs/ihacde.h>
#include <mvs/wto.h>
#include <mvs/link.h>
#include <ctype.h>

__asm__("\n&FUNC    SETC '__delete'");
int __delete(const char *module)
{
    int             rc        = 0;
    unsigned char   name[12]  = {0};
    int             i;

    if (!module || *module <= ' ') goto quit;

    for(i=0; i < 8; i++) {
        if (*module) {
            name[i] = toupper(*module);
            module++;
        }
        else {
            name[i] = ' ';
        }
    }

    /* delete module from memory */
__asm__( "DELETE EPLOC=(%1)\n\t"
         "LR\t%0,15         save return code"
        : "=r"(rc) : "r"(name) : "0", "1", "14", "15", "memory");

quit:
    return rc;
}
