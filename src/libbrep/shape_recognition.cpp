#include "common.h"
#include "brep.h"
#include "shape_recognition.h"
#include "bu/log.h"
curve_t
GetCurveType(ON_Curve *curve)
{
    /* First, see if the curve is planar */
/*    if (!curve->IsPlanar()) {
	return CURVE_GENERAL;
    }*/

    /* Check types in order of increasing complexity - simple
     * is better, so try simple first */
    ON_BoundingBox c_bbox;
    curve->GetTightBoundingBox(c_bbox);
    if (c_bbox.IsDegenerate() == 3) return CURVE_POINT;

    if (curve->IsLinear()) return CURVE_LINE;

    ON_Arc arc;
    if (curve->IsArc(NULL, &arc, 0.01)) {
	if (arc.IsCircle()) return CURVE_CIRCLE;
	ON_Circle circ(arc.StartPoint(), arc.MidPoint(), arc.EndPoint());
	bu_log("arc's circle: center %f, %f, %f   radius %f\n", circ.Center().x, circ.Center().y, circ.Center().z, circ.Radius());
       	return CURVE_ARC;
    }

    // TODO - looks like we need a better test for this...
    if (curve->IsEllipse(NULL, NULL, 0.01)) return CURVE_ELLIPSE;

    return CURVE_GENERAL;
}

surface_t
GetSurfaceType(ON_Surface *surface)
{
    if (surface->IsPlanar(NULL, 0.01)) return SURFACE_PLANE;
    if (surface->IsSphere(NULL, 0.01)) return SURFACE_SPHERE;
    if (surface->IsCylinder(NULL, 0.01)) return SURFACE_CYLINDER;
    if (surface->IsCone(NULL, 0.01)) return SURFACE_CONE;
    if (surface->IsTorus(NULL, 0.01)) return SURFACE_TORUS;
    return SURFACE_GENERAL;
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
