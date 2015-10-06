#include "common.h"

#include <set>
#include <map>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "shape_recognition.h"

int
sph_validate_face(const ON_BrepFace *forig, const ON_BrepFace *fcand)
{
    ON_Sphere sorig;
    ON_Surface *ssorig = forig->SurfaceOf()->Duplicate();
    ssorig->IsSphere(&sorig, BREP_SPHERICAL_TOL);
    delete ssorig;

    ON_Sphere scand;
    ON_Surface *sscand = fcand->SurfaceOf()->Duplicate();
    sscand->IsSphere(&scand, BREP_SPHERICAL_TOL);
    delete sscand;

    // Make sure the sphere centers match
    if (sorig.Center().DistanceTo(scand.Center()) > VUNITIZE_TOL) return 0;

    // Make sure the radii match
    if (!NEAR_ZERO(sorig.Radius() - scand.Radius(), VUNITIZE_TOL)) return 0;

    // TODO - make sure negative/positive status for both faces is the same.

    return 1;
}


/* Return -1 if the sphere face is pointing in toward the center,
 * 1 if it is pointing out, and 0 if there is some other problem */
int
negative_sphere(const ON_Brep *brep, int face_index, double sph_tol)
{
    int ret = 0;
    const ON_Surface *surf = brep->m_F[face_index].SurfaceOf();
    ON_Sphere sph;
    ON_Surface *cs = surf->Duplicate();
    cs->IsSphere(&sph, sph_tol);
    delete cs;

    ON_3dPoint pnt;
    ON_3dVector normal;
    double u = surf->Domain(0).Mid();
    double v = surf->Domain(1).Mid();
    if (!surf->EvNormal(u, v, pnt, normal)) return 0;

    ON_3dVector vect = pnt - sph.Center();
    double dotp = ON_DotProduct(vect, normal);

    if (NEAR_ZERO(dotp, VUNITIZE_TOL)) return 0;
    ret = (dotp < 0) ? -1 : 1;
    if (brep->m_F[face_index].m_bRev) ret = -1 * ret;
    return ret;
}


int
sph_implicit_plane(const ON_Brep *UNUSED(brep), int UNUSED(ec), int *UNUSED(edges))
{
    return -1;
}

int
sph_implicit_params(struct subbrep_shoal_data *data, ON_SimpleArray<ON_Plane> *sph_planes, int shoal_nonplanar_face)
{
    ON_Sphere sph;
    ON_Surface *cs = data->i->brep->m_L[data->shoal_loops[0]].Face()->SurfaceOf()->Duplicate();
    cs->IsSphere(&sph, BREP_SPHERICAL_TOL);
    delete cs;

    int need_arbn = ((*sph_planes).Count() == 0) ? 0 : 1;

    // Populate the CSG implicit primitive data
    data->params->csg_type = SPHERE;
    // Flag the sphere according to the negative or positive status of the
    // sphere surface.
    data->params->negative = negative_sphere(data->i->brep, shoal_nonplanar_face, BREP_SPHERICAL_TOL);
    data->params->bool_op = (data->params->negative == -1) ? '-' : 'u';

    ON_3dPoint origin = sph.Center();
    BN_VMOVE(data->params->origin, origin);
    data->params->radius = sph.Radius();

    if (need_arbn) {
	ON_3dVector xplus = sph.PointAt(0,0) - sph.Center();
	ON_3dVector yplus = sph.PointAt(M_PI/2,0) - sph.Center();
	ON_3dVector zplus = sph.NorthPole() - sph.Center();
	ON_3dPoint xmax = sph.Center() + 1.01*xplus;
	ON_3dPoint xmin = sph.Center() - 1.01*xplus;
	ON_3dPoint ymax = sph.Center() + 1.01*yplus;
	ON_3dPoint ymin = sph.Center() - 1.01*yplus;
	ON_3dPoint zmax = sph.Center() + 1.01*zplus;
	ON_3dPoint zmin = sph.Center() - 1.01*zplus;
	xplus.Unitize();
	yplus.Unitize();
	zplus.Unitize();
	(*sph_planes).Append(ON_Plane(xmin, -1 * xplus));
	(*sph_planes).Append(ON_Plane(xmax, xplus));
	(*sph_planes).Append(ON_Plane(ymin, -1 * yplus));
	(*sph_planes).Append(ON_Plane(ymax, yplus));
	(*sph_planes).Append(ON_Plane(zmin, -1 * zplus));
	(*sph_planes).Append(ON_Plane(zmax, zplus));
    }

    return need_arbn;
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
