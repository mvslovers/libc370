#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct member {
    char name[9];
    int  size;
};

/* bsearch() passes the element first and the key second, qsort() two
   elements: a comparison of two struct member serves both */
static int byname(const void *a, const void *b)
{
    return strcmp(((const struct member *)a)->name,
                  ((const struct member *)b)->name);
}

int main(void)
{
    struct member dir[] = {
        { "IEFBR14", 1 }, { "ASMA90", 220 }, { "IEBCOPY", 60 },
    };
    size_t n = sizeof dir / sizeof dir[0];
    struct member key, *hit;

    qsort(dir, n, sizeof dir[0], byname);

    strcpy(key.name, "IEBCOPY");
    hit = bsearch(&key, dir, n, sizeof dir[0], byname);
    if (hit != NULL)
        printf("%s is %d tracks\n", hit->name, hit->size);
    return 0;
}
