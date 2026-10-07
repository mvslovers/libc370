#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Keep every line of DD INPUT in storage. Each line is copied
   into a pool of 32 KB instead of a block of its own, so a
   line of 10 characters costs 11 bytes, not 64. */

#define POOLSIZE 32768

struct pool {
    struct pool *next;
    size_t       used;
    char         data[POOLSIZE];
};

static char *keep(struct pool **head, const char *s)
{
    size_t       len = strlen(s) + 1;
    struct pool *p = *head;
    char        *copy;

    if (len > POOLSIZE)
        return NULL;
    if (p == NULL || POOLSIZE - p->used < len) {
        p = malloc(sizeof *p);
        if (p == NULL)
            return NULL;
        p->next = *head;
        p->used = 0;
        *head = p;
    }
    copy = p->data + p->used;
    memcpy(copy, s, len);
    p->used += len;
    return copy;
}

static void release(struct pool *p)
{
    struct pool *next;

    for (; p != NULL; p = next) {
        next = p->next;
        free(p);
    }
}

int main(void)
{
    struct pool *pools = NULL;
    char         line[258];
    long         n = 0;
    FILE        *in = fopen("DD:INPUT", "r");

    if (in == NULL)
        return 16;
    while (fgets(line, sizeof line, in) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        if (keep(&pools, line) == NULL) {
            fputs("POOL: out of storage\n", stderr);
            break;
        }
        n++;
    }
    fclose(in);

    printf("%ld lines kept\n", n);
    release(pools);                     /* every block, explicitly */
    return 0;
}
