#ifndef PULLBACK_CURVE
#define PULLBACK_CURVE

#include "opennurbs.h"


/*                  p u l l b a c k _ c u r v e
 *
 * Pull an arbitrary model-space *curve* onto the given *surface* as a
 * curve within the surface's domain when, for each point c = C(t) on
 * the curve and the closest point s = S(u,v) on the surface, we have:
 * distance(c,s) <= *tolerance*.
 *
 * The resulting 2-dimensional curve will be approximated using the
 * following process:
 *
 * 1. Adaptively sample the 3d curve in the domain of the surface
 * (ensure tolerance constraint). Sampling terminates when the
 * following flatness criterion is met:
 *    given two parameters on the curve t1 and t2 (which map to points p1 and p2 on the curve)
 *    let m be a parameter randomly chosen near the middle of the interval [t1,t2]
 *                                                              ____
 *    then the curve between t1 and t2 is flat if distance(C(m),p1p2) < flatness
 *
 * 2. Use the sampled points to perform a global interpolation using
 *    universal knot generation to build a B-Spline curve.
 *
 * 3. If the curve is a line or an arc (determined with openNURBS routines),
 *    return the appropriate ON_Curve subclass (otherwise, return an ON_NurbsCurve).
 *
 *
 */
extern ON_Curve*
pullback_curve(const ON_Surface* surface,
	       const ON_Curve* curve,
	       double tolerance = 1.0e-6,
	       double flatness = 1.0e-3);


#endif
