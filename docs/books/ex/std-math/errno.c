#include <errno.h>
#include <math.h>
#include <stdio.h>

static double root(double x)
{
    double r;

    errno = 0;
    r = sqrt(x);
    if (errno == EDOM) {
        printf("sqrt(%g): argument out of domain\n", x);
        return 0.0;
    }
    return r;
}

int main(void)
{
    double a = 3.0, b = 4.0;

    printf("hypotenuse %.6f\n", root(a * a + b * b));
    root(-1.0);
    return 0;
}
