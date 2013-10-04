/*
 * Implementation of C99 function "double hypot()" from:
 *
 *   http://en.wikipedia.org/wiki/Hypot
 *
 */

#include <math.h>

double
hypot(double x, double y)
{
    double t;
    x = abs(x);
    y = abs(y);
    t = min(x,y);
    x = max(x,y);
    t = t/x;
    return x*sqrt(1+t*t);
}
