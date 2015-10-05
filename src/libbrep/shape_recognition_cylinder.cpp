#include "common.h"

#include <set>
#include <map>
#include <limits>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "shape_recognition.h"

#define L3_OFFSET 6
#define L4_OFFSET 8


int
cyl_validate_face(const ON_BrepFace *forig, const ON_BrepFace *fcand)
{
    ON_Cylinder corig;
    ON_Surface *csorig = forig->SurfaceOf()->Duplicate();
    csorig->IsCylinder(&corig, BREP_CYLINDRICAL_TOL);
    delete csorig;
    ON_Line lorig(corig.circle.Center(), corig.circle.Center() + corig.Axis());

    ON_Cylinder ccand;
    ON_Surface *cscand = fcand->SurfaceOf()->Duplicate();
    cscand->IsCylinder(&ccand, BREP_CYLINDRICAL_TOL);
    delete cscand;
    double d1 = lorig.DistanceTo(ccand.circle.Center());
    double d2 = lorig.DistanceTo(ccand.circle.Center() + ccand.Axis());

    // Make sure the cylinder axes are colinear
    if (corig.Axis().IsParallelTo(ccand.Axis(), VUNITIZE_TOL) == 0) return 0;
    if (fabs(d1) > BREP_CYLINDRICAL_TOL) return 0;
    if (fabs(d2) > BREP_CYLINDRICAL_TOL) return 0;

    // Make sure the radii are the same
    if (!NEAR_ZERO(corig.circle.Radius() - ccand.circle.Radius(), VUNITIZE_TOL)) return 0;

    // TODO - make sure negative/positive status for both cyl faces is the same.

    return 1;
}


/* Return -1 if the cylinder face is pointing in toward the cylinder axis,
 * 1 if it is pointing out, and 0 if there is some other problem */
int
negative_cylinder(const ON_Brep *brep, int face_index, double cyl_tol) {
    int ret = 0;
    const ON_Surface *surf = brep->m_F[face_index].SurfaceOf();
    ON_Cylinder cylinder;
    ON_Surface *cs = surf->Duplicate();
    cs->IsCylinder(&cylinder, cyl_tol);
    delete cs;

    ON_3dPoint pnt;
    ON_3dVector normal;
    double u = surf->Domain(0).Mid();
    double v = surf->Domain(1).Mid();
    if (!surf->EvNormal(u, v, pnt, normal)) return 0;
    ON_3dPoint axis_pnt = cylinder.circle.Center();

    ON_3dVector axis_vect = pnt - axis_pnt;
    double dotp = ON_DotProduct(axis_vect, normal);

    if (NEAR_ZERO(dotp, VUNITIZE_TOL)) return 0;
    ret = (dotp < 0) ? -1 : 1;
    if (brep->m_F[face_index].m_bRev) ret = -1 * ret;
    return ret;
}

int
cyl_implicit_plane(const ON_Brep *brep, int lc, int *le, ON_SimpleArray<ON_Plane> *cyl_planes)
{
    std::set<int> linear_edges;
    std::set<int>::iterator c_it;
    array_to_set(&linear_edges, le, lc);
    // If we have two non-linear edges remaining, construct a plane from them.
    // If we have a count other than two or zero, return.
    if (linear_edges.size() != 2 && linear_edges.size() != 0)
       	return -1;
    if (linear_edges.size() == 2 ) {
	std::set<int> verts;
	// If both edges share a pre-existing face that is planar, use the inverse of that
	// plane.  Otherwise, construct a plane from the vertex points.
	//bu_log("found two linear edges\n");
	for (c_it = linear_edges.begin(); c_it != linear_edges.end(); c_it++) {
	    const ON_BrepEdge *edge = &(brep->m_E[*c_it]);
	    verts.insert(edge->m_vi[0]);
	    verts.insert(edge->m_vi[1]);
	}
	ON_3dPointArray points;
	for (c_it = verts.begin(); c_it != verts.end(); c_it++) {
	    const ON_BrepVertex *v = &(brep->m_V[*c_it]);
	    points.Append(v->Point());
	}

	ON_Plane pf(points[0], points[1], points[2]);

	// If the fourth point is not coplanar with the other three, we've hit a case
	// that we don't currently handlinear_edges.- hault. (TODO - need test case)
	if (pf.DistanceTo(points[3]) > BREP_PLANAR_TOL) return -1;

	(*cyl_planes).Append(pf);
	return (*cyl_planes).Count() - 1;
    }

    return 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
