#include <stdio.h>
#include <errno.h>
#include <ext/array.h>
#include <mvs/dslist.h>

/* Show the free space of the online disk volumes. */
int main(void)
{
    VOLLIST  **vols;
    unsigned i, n;

    vols = __listvl(NULL, 1, NULL);     /* all volumes, with LSPACE */
    if (!vols) {
        printf(errno ? "no storage\n" : "no volume online\n");
        return 8;
    }

    printf("VOLSER  CUU  TYPE  FREE CYL  FREE TRK  LARGEST\n");
    n = arraycount(&vols);
    for (i = 0; i < n; i++) {
        VOLLIST *v = vols[i];

        printf("%-6s  %03X  %04X  %8u  %8u  %4u/%u\n", v->volser, v->cuu,
               v->dasdtype, v->freecyls, v->freetrks,
               v->maxfreecyls, v->maxfreetrks);
    }

    __freevl(&vols);
    return 0;
}
