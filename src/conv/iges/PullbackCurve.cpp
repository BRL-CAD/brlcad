#include "PullbackCurve.h"
#include <assert.h>
#include <vector>
#include <limits>

using namespace std;

#define RANGE_HI 0.55
#define RANGE_LO 0.45
#define UNIVERSAL_SAMPLE_COUNT 1001

numeric_limits<double> real;

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
  assert(lo < hi);
  double random_pos = drand48() * (RANGE_HI - RANGE_LO) + RANGE_LO;
  double newt = random_pos * (hi - lo) + lo; 
  toUV(data, out, newt);
  return newt;
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

typedef struct _bspline {  
  int p; // degree
  int m; // num_knots-1
  int n; // num_samples-1 (aka number of control points)
  vector<double> params;
  vector<double> knots;
  ON_2dPointArray controls;
} BSpline;

void
generateKnots(BSpline& bspline) {
  int num_knots = bspline.m + 1;
  bspline.knots.reserve(num_knots);
  for (int i = 0; i <= bspline.p; i++) {
    bspline.knots[i] = 0.0;    
  }
  for (int i = bspline.m-bspline.p; i <= bspline.m; i++) {
    bspline.knots[i] = 1.0;    
  }
  for (int i = 1; i <= bspline.n-bspline.p; i++) {
    bspline.knots[bspline.p+i] = (double)i / (bspline.n-bspline.p+1.0);
  }
}

int 
getKnotInterval(BSpline& bspline, double u) {
  int k = 0; 
  while (u >= bspline.knots[k]) k++;
  k = (k == 0) ? k : k-1;
  return k;
}

int
getCoefficients(BSpline& bspline, vector<double>& N, double u) {
  // evaluate the b-spline basis function for the given parameter u
  // place the results in N[]
  for (int i = 0; i < bspline.n+1; i++) {
    N[i] = 0.0;
  }
  if (u == bspline.knots[0]) {
    N[0] = 1.0;
    return 0;
  } else if (u == bspline.knots[bspline.m]) {
    N[bspline.n] = 1.0;
    return bspline.n;
  }
  int k = getKnotInterval(bspline, u);
  N[k] = 1.0;
  for (int d = 1; d <= bspline.p; d++) {
    double uk_1 = bspline.knots[k+1];
    double uk_d_1 = bspline.knots[k-d+1];
    N[k-d] = ((uk_1 - u)/(uk_1 - uk_d_1)) * N[k-d+1];
    for (int i = k-d+1; i <= k-1; i++) {
      double ui = bspline.knots[i];
      double ui_1 = bspline.knots[i+1];
      double ui_d = bspline.knots[i+d];
      double ui_d_1 = bspline.knots[i+d+1];
      N[i] = ((u - ui)/(ui_d - ui)) * N[i] + ((ui_d_1 - u)/(ui_d_1 - ui_1))*N[i+1];
    }
    double uk = bspline.knots[k];
    double uk_d = bspline.knots[k+d];
    N[k] = ((u - uk)/(uk_d - uk)) * N[k];
  }
  return k;
}

// XXX: this function sucks...
void
generateParameters(BSpline& bspline) {
  double lastT = 0.0;
  vector< vector<double> > N;
  bspline.params.reserve(bspline.n+1);
  N.reserve(UNIVERSAL_SAMPLE_COUNT);
  for (int i = 0; i < UNIVERSAL_SAMPLE_COUNT; i++) {
    N[i].reserve(bspline.n);
  }
  for (int i = 0; i < UNIVERSAL_SAMPLE_COUNT; i++) {
    double t = (double)i / (UNIVERSAL_SAMPLE_COUNT-1);
    getCoefficients(bspline, N[i], t);
  }
  for (int i = 0; i < bspline.n+1; i++) {
    double max = real.min();
    for (int j = 0; j < UNIVERSAL_SAMPLE_COUNT; j++) {
      double f = N[j][i];
      double t = (double)j/(UNIVERSAL_SAMPLE_COUNT-1);
      if (f > max) {
	max = f;
	if (j == UNIVERSAL_SAMPLE_COUNT-1) bspline.params[i] = t;
      } else if (f < max) {
	bspline.params[i] = lastT;
	break;
      }
      lastT = t;
    }
  }
}

void
generateControlPoints(BSpline& bspline, PBCData& data)
{

}

ON_NurbsCurve*
newNURBSCurve(BSpline& spline) {
  return NULL;
}

ON_Curve* 
interpolateCurve(PBCData& data) {
  if (data.samples.Count() == 2) {
    // build a line
    return new ON_LineCurve(data.samples[0], data.samples[1]);
  } else {
    // build a NURBS curve, then see if it can be simplified!
    BSpline spline;
    spline.p = 3;
    spline.n = data.samples.Count()-1;
    spline.m = spline.n + spline.p + 1;
    generateKnots(spline);
    generateParameters(spline);
    generateControlPoints(spline, data);
    ON_NurbsCurve* nurbs = newNURBSCurve(spline);
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

  return interpolateCurve(data);
}
