#include "opennurbs_ext.h"
#include <assert.h>
#include <vector>
#include <limits>
#include "tnt.h"
#include "jama_lu.h"

#include "machine.h"
#include "vmath.h"
#include "bu.h"
#include "vector.h"

#define RANGE_HI 0.55
#define RANGE_LO 0.45
#define UNIVERSAL_SAMPLE_COUNT 1001

using namespace std;

#define TRACE(m) cerr << m << endl;

namespace brlcad {  

  //--------------------------------------------------------------------------------
  // SurfaceTree
  SurfaceTree::SurfaceTree(ON_BrepFace* face) 
    : m_face(face)
  {
    // build the surface bounding volume hierarchy
    const ON_Surface* surf = face->SurfaceOf();
    ON_Interval u = surf->Domain(0);
    ON_Interval v = surf->Domain(1);
    m_root = subdivideSurface(u, v, 0);
    TRACE("m_root: " << m_root);
  }
  
  SurfaceTree::~SurfaceTree() {
    delete m_root;
  }

  BBNode* 
  SurfaceTree::getRootNode() const {
    return m_root;
  }

  ON_2dPoint
  SurfaceTree::getClosestPointEstimate(const ON_3dPoint& pt) {
    return m_root->getClosestPointEstimate(pt);
  }

  BBNode*
  SurfaceTree::surfaceBBox(bool isLeaf, const ON_Interval& u, const ON_Interval& v)
  {
    TRACE("brep_surface_bbox");
    ON_3dPoint corners[4];
    const ON_Surface* surf = m_face->SurfaceOf();

    if (!surf->EvPoint(u.Min(),v.Min(),corners[0]) ||
	!surf->EvPoint(u.Max(),v.Min(),corners[1]) ||
	!surf->EvPoint(u.Max(),v.Max(),corners[2]) ||
	!surf->EvPoint(u.Min(),v.Max(),corners[3])) {
      bu_bomb("Could not evaluate a point on surface"); // XXX fix this message
    }

    point_t min, max;
    VSETALL(min, MAX_FASTF);
    VSETALL(max, -MAX_FASTF);
    for (int i = 0; i < 4; i++) 
      VMINMAX(min,max,((double*)corners[i]));
            
    // calculate the estimate point on the surface: i.e. use the point
    // on the surface defined by (u.Mid(), v.Mid()) as a heuristic for
    // finding the uv domain bounding a portion of the surface close
    // to an arbitrary model-space point (see
    // getClosestPointEstimate())
    ON_3dPoint estimate;
    if (!surf->EvPoint(u.Mid(),v.Mid(),estimate)) {
      bu_bomb("Could not evaluate estimate point on surface");
    }

    BBNode* node;
    if (isLeaf) 
      node = new SubsurfaceBBNode(ON_BoundingBox(ON_3dPoint(min),
						 ON_3dPoint(max)), 
				  m_face, 
				  u, v);
    else 
      node = new BBNode(ON_BoundingBox(ON_3dPoint(min),
				       ON_3dPoint(max)));
    node->m_estimate = estimate;
    return node;
  }

  BBNode*
  SurfaceTree::subdivideSurface(const ON_Interval& u, 
				const ON_Interval& v, 
				int depth)
  {
    TRACE("brep_surface_subdivide");
    const ON_Surface* surf = m_face->SurfaceOf();
    if (isFlat(surf, u, v) || depth >= BREP_MAX_FT_DEPTH) {
      return surfaceBBox(true, u, v);
    } else {
      BBNode* parent = surfaceBBox(false, u, v);
      BBNode* quads[4];
      ON_Interval first(0,0.5);
      ON_Interval second(0.5,1.0);
      quads[0] = subdivideSurface(u.ParameterAt(first),  v.ParameterAt(first),  depth+1);
      quads[1] = subdivideSurface(u.ParameterAt(second), v.ParameterAt(first),  depth+1);
      quads[2] = subdivideSurface(u.ParameterAt(second), v.ParameterAt(second), depth+1);
      quads[3] = subdivideSurface(u.ParameterAt(first),  v.ParameterAt(second), depth+1);

      for (int i = 0; i < 4; i++)
	parent->addChild(quads[i]);
      
      return parent;
    }
  }

  /**
   * Determine whether a given surface is flat enough, i.e. it falls
   * beneath our simple flatness constraints. The flatness constraint in
   * this case is a sampling of normals across the surface such that
   * the product of their combined dot products is close to 1.
   *
   * \Product_{i=1}^{7} n_i \dot n_{i+1} = 1
   *
   * Would be a perfectly flat surface. Generally something in the range
   * 0.8-0.9 should suffice (according to Abert, 2005).
   *
   *   	 +-------------------------+
   *	 |           	           |
   *	 |            +            |
   *	 |                         |
   *  V  |       +         +       |
   *	 |                         |
   *	 |            +            |
   *	 |                         |
   *	 +-------------------------+
   *                  U
   *                     
   * The "+" indicates the normal sample.
   */
  bool
  SurfaceTree::isFlat(const ON_Surface* surf, const ON_Interval& u, const ON_Interval& v)
  {
    ON_3dVector normals[8];    

    bool fail = false;
    
    if (surf->IsAtSingularity(u.Min(), v.Min()) ||
	surf->IsAtSingularity(u.Max(), v.Min()) ||
	surf->IsAtSingularity(u.Max(), v.Max()) ||
	surf->IsAtSingularity(u.Min(), v.Max())) {
      TRACE("singularity! --------------------------");
      TRACE("umin: " << u.Min());
      TRACE("umax: " << u.Max());
      TRACE("vmin: " << v.Min());
      TRACE("vmax: " << v.Max());
    }

    // corners    
    if (!surf->EvNormal(u.Min(),v.Min(),normals[0]) ||
	!surf->EvNormal(u.Max(),v.Min(),normals[1]) ||
	!surf->EvNormal(u.Max(),v.Max(),normals[2]) ||
	!surf->EvNormal(u.Min(),v.Max(),normals[3]) ||

	// interior
	!surf->EvNormal(u.ParameterAt(0.5),v.ParameterAt(0.25),normals[4]) ||
	!surf->EvNormal(u.ParameterAt(0.75),v.ParameterAt(0.5),normals[5]) ||
	!surf->EvNormal(u.ParameterAt(0.5),v.ParameterAt(0.75),normals[6]) ||
	!surf->EvNormal(u.ParameterAt(0.25),v.ParameterAt(0.5),normals[7])) {
      bu_bomb("Could not evaluate a normal on the surface"); // XXX fix this
    }

    double product = 1.0;
    
#ifdef DO_VECTOR    
    double ax[4] VEC_ALIGN;
    double ay[4] VEC_ALIGN;
    double az[4] VEC_ALIGN;

    double bx[4] VEC_ALIGN;
    double by[4] VEC_ALIGN;
    double bz[4] VEC_ALIGN;

    distribute(4, normals, ax, ay, az);
    distribute(4, &normals[1], bx, by, bz);
    
    // how to get the normals in here?
    {
      dvec<4> xa(ax);
      dvec<4> ya(ay);
      dvec<4> za(az);
      dvec<4> xb(bx);
      dvec<4> yb(by);
      dvec<4> zb(bz);
      dvec<4> dots = xa * xb + ya * yb + za * zb;
      product *= dots.foldr(1,dvec<4>::mul());
      if (product < 0.0) return false;
    }    
    // try the next set of normals
    {
      distribute(3, &normals[4], ax, ay, az);
      distribute(3, &normals[5], bx, by, bz);
      dvec<4> xa(ax);
      dvec<4> xb(bx);
      dvec<4> ya(ay); 
      dvec<4> yb(by);
      dvec<4> za(az);
      dvec<4> zb(bz);
      dvec<4> dots = xa * xb + ya * yb + za * zb;
      product *= dots.foldr(1,dvec<4>::mul(),3);
    }
#else
    for (int i = 0; i < 7; i++) {
      product *= (normals[i] * normals[i+1]);
    }
#endif

    return product >= BREP_SURFACE_FLATNESS;
  }

  //--------------------------------------------------------------------------------
  // get_closest_point implementation

  typedef struct _gcp_data {
    const ON_Surface* surf;
    ON_3dPoint  pt;

    // for newton iteration
    ON_3dPoint  S;
    ON_3dVector du;
    ON_3dVector dv;
    ON_3dVector duu;
    ON_3dVector dvv;
    ON_3dVector duv;
  } GCPData;

  bool
  gcp_gradient(pt2d_t out_grad, GCPData& data, pt2d_t uv) {
    bool evaluated = data.surf->Ev2Der(uv[0], 
				       uv[1], 
				       data.S, 
				       data.du, 
				       data.dv, 
				       data.duu, 
				       data.duv, 
				       data.dvv); // calc S(u,v) dS/du dS/dv d2S/du2 d2S/dv2 d2S/dudv
    if (!evaluated) return false;   
    out_grad[0] = 2 * (data.du * (data.S - data.pt));
    out_grad[1] = 2 * (data.dv * (data.S - data.pt));
    return true;
  }

  bool
  gcp_newton_iteration(pt2d_t out_uv, GCPData& data, pt2d_t grad, pt2d_t in_uv) {
    ON_3dVector delta = data.S - data.pt;
    double g1du = 2 * (data.duu  * delta) + 2 * (data.du * data.du);
    double g2du = 2 * (data.duv  * delta) + 2 * (data.dv * data.du);
    double g1dv = g2du;
    double g2dv = 2 * (data.dvv  * delta) + 2 * (data.dv * data.dv); 
    mat2d_t jacob = { g1du, g1dv,
		      g2du, g2dv };
    mat2d_t inv_jacob;
    if (mat2d_inverse(inv_jacob, jacob)) {
      pt2d_t tmp;
      mat2d_pt2d_mul(tmp, inv_jacob, grad);
      pt2dsub(out_uv, in_uv, tmp);
      return true;
    } else {
      cerr << "inverse failed!" << endl; // XXX fix the error handling
      return false;
    }   
  }

  bool
  get_closest_point(ON_2dPoint& outpt,
		    ON_BrepFace* face,
		    const ON_3dPoint& point,
		    SurfaceTree* tree,
		    double tolerance) {
    
    bool found = false;
    double d_last = real.max();
    pt2d_t curr_grad;
    pt2d_t new_uv;
    GCPData data;
    data.surf = face->SurfaceOf();
    data.pt = point;

    // get initial estimate
    SurfaceTree* a_tree = (tree == NULL) ? new SurfaceTree(face) : tree;
    ON_2dPoint est = a_tree->getClosestPointEstimate(point);
    pt2d_t uv = { est[0], est[1] };

    // do the newton iterations 
    // terminates on 1 of 3 conditions:
    // 1. if the gradient falls below an epsilon (preferred :-)
    // 2. if the gradient diverges
    // 3. iterated MAX_FCP_ITERATIONS
    for (int i = 0; i < BREP_MAX_FCP_ITERATIONS; i++) {
      gcp_gradient(curr_grad, data, uv);
      double d = v2mag(curr_grad);
      if (d < BREP_FCP_ROOT_EPSILON) {
	found = true; break;
      } else if (d > d_last) {
	break;
      }
      gcp_newton_iteration(new_uv, data, curr_grad, uv);
      move(uv, new_uv);
      d_last = d;
    }
    if (found) {
      // check to see if we've left the surface domain
      double l, h;
      data.surf->GetDomain(0,&l,&h);
      if (uv[0] < l) uv[0] = l; // clamp if out of range!
      if (uv[0] > h) uv[0] = h;
      data.surf->GetDomain(1,&l,&h);
      if (uv[1] < l) uv[1] = l;
      if (uv[1] > h) uv[1] = h;
      
      outpt[0] = uv[0];
      outpt[1] = uv[1];
    }

    return found;
  }


  //--------------------------------------------------------------------------------
  // pullback_curve implementation
  typedef struct pbc_data {
    double tolerance;
    double flatness;
    const ON_Curve* curve;
    const ON_Surface* surf;
    ON_BrepFace* face;
    SurfaceTree* tree;
    ON_2dPointArray samples;
  } PBCData;

  typedef struct _bspline {  
    int p; // degree
    int m; // num_knots-1
    int n; // num_samples-1 (aka number of control points)
    vector<double> params;
    vector<double> knots;
    ON_2dPointArray controls;
  } BSpline;

  bool
  isFlat(const ON_2dPoint& p1, const ON_2dPoint& m, const ON_2dPoint& p2, double flatness) {
    ON_Line line = ON_Line(ON_3dPoint(p1), ON_3dPoint(p2));
    return line.DistanceTo(ON_3dPoint(m)) <= flatness;
  }

  bool 
  toUV(PBCData& data, ON_2dPoint& out_pt, double t) {
    ON_3dPoint pointOnCurve = data.curve->PointAt(t);
    return get_closest_point(out_pt, data.face, pointOnCurve, data.tree);
  }

  double
  randomPointFromRange(PBCData& data, ON_2dPoint& out, double lo, double hi)
  {
    assert(lo < hi);
    double random_pos = drand48() * (RANGE_HI - RANGE_LO) + RANGE_LO;
    double newt = random_pos * (hi - lo) + lo; 
    assert(toUV(data, out, newt));
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
  getCoefficients(BSpline& bspline, Array1D<double>& N, double u) {
    // evaluate the b-spline basis function for the given parameter u
    // place the results in N[]
    N = 0.0;
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
    bspline.params.reserve(bspline.n+1);
    Array2D<double> N(UNIVERSAL_SAMPLE_COUNT, bspline.n+1);
    for (int i = 0; i < UNIVERSAL_SAMPLE_COUNT; i++) {
      double t = (double)i / (UNIVERSAL_SAMPLE_COUNT-1);
      Array1D<double> n = Array1D<double>(N.dim2(), N[i]);
      getCoefficients(bspline, n, t);
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
  printMatrix(Array2D<double>& m) {
    printf("---\n");
    for (int i = 0; i < m.dim1(); i++) {
      for (int j = 0; j < m.dim2(); j++) {
	printf("% 5.5f ", m[i][j]);
      }
      printf("\n");
    }  
  }

  void
  generateControlPoints(BSpline& bspline, PBCData& data)
  {
    Array2D<double> bigN(bspline.n+1, bspline.n+1);
    for (int i = 0; i < bspline.n+1; i++) {
      Array1D<double> n = Array1D<double>(bigN.dim2(), bigN[i]);
      getCoefficients(bspline, n, bspline.params[i]);
    }
    Array2D<double> bigD(bspline.n+1,2);
    for (int i = 0; i < bspline.n+1; i++) {
      bigD[i][0] = data.samples[i].x;
      bigD[i][1] = data.samples[i].y;
    }
  
    printMatrix(bigD);
    printMatrix(bigN);

    JAMA::LU<double> lu(bigN);
    assert(lu.isNonsingular() > 0);
    Array2D<double> bigP = lu.solve(bigD); // big linear algebra black box here...
  
    // extract the control points
    for (int i = 0; i < bspline.n+1; i++) {
      ON_2dPoint& p = bspline.controls.AppendNew();
      p.x = bigP[i][0];
      p.y = bigP[i][1];
    }
  }

  ON_NurbsCurve*
  newNURBSCurve(BSpline& spline) {
    // we now have everything to complete our spline
    ON_NurbsCurve* c = ON_NurbsCurve::New(3,
					  false,
					  spline.p+1,
					  spline.n+1);
    c->ReserveKnotCapacity(spline.knots.size());
    for (int i = 0; i < spline.knots.size(); i++) {
      c->m_knot[i] = spline.knots[i];
    }
  
    for (int i = 0; i < spline.controls.Count(); i++) {
      c->SetCV(i, ON_3dPoint(spline.controls[i]));
    }

    return c;
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
    
      // XXX - attempt to simplify here!

      return nurbs;
    }
  }

  ON_Curve*
  pullback_curve(ON_BrepFace* face,		 
		 const ON_Curve* curve,
		 SurfaceTree* tree,
		 double tolerance, 
		 double flatness) {
    PBCData data;
    data.tolerance = tolerance;
    data.flatness = flatness;
    data.curve = curve;
    data.face = face;
    data.surf = face->SurfaceOf();
    data.tree = tree;

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

    for (int i = 0; i < data.samples.Count(); i++) {
      cerr << data.samples[i].x << "," << data.samples[i].y << endl;
    }

    return interpolateCurve(data);
  }


}
