static int count;           /* static        */
static int limit = 10;      /* static, init. */
const char *msg = "hello";  /* global        */

int next(void)
{
    return ++count < limit;
}
