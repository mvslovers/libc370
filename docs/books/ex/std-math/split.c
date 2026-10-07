#include <math.h>
#include <stdio.h>

int main(void)
{
    double ipart, frac;
    double x = -7.25;

    frac = modf(x, &ipart);                 /* -7.00 and -0.25 */
    printf("%.2f + %.2f\n", ipart, frac);
    printf("floor %.2f, ceil %.2f, fmod %.2f\n",
           floor(x), ceil(x), fmod(x, 2.0)); /* -8.00 -7.00 -1.25 */
    return 0;
}
