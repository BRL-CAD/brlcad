
// Otherwise #defines like M_PI are undeclared under Visual Studio
#define _USE_MATH_DEFINES

#include <exception>
#ifdef WIN32
# define _USE_MATH_DEFINES
#endif
#include <math.h>
#include <float.h>
#include "shapes.h"
#include "utils.h"

namespace p2t {

const double PI_3div4 = 3 * M_PI / 4;
const double PI_div2 = 1.57079632679489661923;
const double EPSILON = DBL_MIN;

/**
 * Forumla to calculate signed area<br>
 * Positive if CCW<br>
 * Negative if CW<br>
 * 0 if collinear<br>
 * <pre>
 * A[P1,P2,P3]  =  (x1*y2 - y1*x2) + (x2*y3 - y2*x3) + (x3*y1 - y3*x1)
 *              =  (x1-x3)*(y2-y3) - (y1-y3)*(x2-x3)
 * </pre>
 */
Orientation Orient2d(Point& pa, Point& pb, Point& pc)
{
  double detleft = (pa.x - pc.x) * (pb.y - pc.y);
  double detright = (pa.y - pc.y) * (pb.x - pc.x);
  double val = detleft - detright;
  if (val > -EPSILON && val < EPSILON) {
    return COLLINEAR;
  } else if (val > 0) {
    return CCW;
  }
  return CW;
}

/*
bool InScanArea(Point& pa, Point& pb, Point& pc, Point& pd)
{
  double pdx = pd.x;
  double pdy = pd.y;
  double adx = pa.x - pdx;
  double ady = pa.y - pdy;
  double bdx = pb.x - pdx;
  double bdy = pb.y - pdy;

  double adxbdy = adx * bdy;
  double bdxady = bdx * ady;
  double oabd = adxbdy - bdxady;

  if (oabd <= EPSILON) {
    return false;
  }

  double cdx = pc.x - pdx;
  double cdy = pc.y - pdy;

  double cdxady = cdx * ady;
  double adxcdy = adx * cdy;
  double ocad = cdxady - adxcdy;

  if (ocad <= EPSILON) {
    return false;
  }

  return true;
}

*/

bool InScanArea(Point& pa, Point& pb, Point& pc, Point& pd)
{
  double oadb = (pa.x - pb.x)*(pd.y - pb.y) - (pd.x - pb.x)*(pa.y - pb.y);
  if (oadb >= -EPSILON) {
    return false;
  }

  double oadc = (pa.x - pc.x)*(pd.y - pc.y) - (pd.x - pc.x)*(pa.y - pc.y);
  if (oadc <= EPSILON) {
    return false;
  }
  return true;
}

}
