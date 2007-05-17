#include "PullbackCurve.h"

typedef struct pbc_data {
  double tolerance;
  double flatness;
  const ON_Curve* curve;
  const ON_Surface* surf;
  ON_2dPointArray samples;
} PBCData;

bool
isFlat(const ON_2dPoint& p1, const ON_2dPoint& m, const ON_2dPoint& p2, double flatness) {
  ON_Line line = ON_Line(ON_3dPoint(p1), ON_3dPoint(p2));
  return line.DistanceTo(ON_3dPoint(m)) <= flatness;
}

bool 
toUV(PBCData& data, ON_2dPoint& out_pt, double t) {
  double u,v;
  ON_3dPoint pointOnCurve = data.curve->PointAt(t);
  if (data.surf->GetClosestPoint(pointOnCurve, &u, &v, data.tolerance)) {
    out_pt.Set(u,v);
    return true;
  } else
    return false;
}

double
randomPointFromRange(PBCData& data, ON_2dPoint& out, double lo, double hi)
{
  return 0.0;
}

bool 
sample(PBCData& data, 
       double t1, 
       double t2, 
       const ON_2dPoint& p1, 
       const ON_2dPoint& p2)
{
  ON_2dPoint m;
  double t = randomPointFromRange(data, m, t1, t2);
  if (isFlat(p1, m, p2, data.flatness)) {
    data.samples.Append(p2);
  } else {
    sample(data, t1, t, p1, m);
    sample(data, t, t2, m, p2);
  }
}

ON_Curve*
pullback_curve(const ON_Surface* surface, 
	       const ON_Curve* curve, 
	       double tolerance, 
	       double flatness) {
  PBCData data;
  data.tolerance = tolerance;
  data.flatness = flatness;
  data.curve = curve;
  data.surf = surface;

  // Step 1 - adaptively sample the curve
  double tmin, tmax;
  data.curve->GetDomain(&tmin, &tmax);
  ON_2dPoint& start = data.samples.AppendNew(); // new point is added to samples and returned
  if (toUV(data, start, tmin)) { return NULL; } // fails if first point is out of tolerance!

  ON_2dPoint p1, p2;
  toUV(data, p1, tmin);
  toUV(data, p2, tmax);
  if (!sample(data, tmin, tmax, p1, p2)) {
    return NULL;
  }

  return NULL;
}
