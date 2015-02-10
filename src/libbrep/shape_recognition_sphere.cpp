#include "common.h"

#include <set>
#include <map>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "shape_recognition.h"

#define pout(p) std::cout << p.x << "," << p.y << "," << p.z;


int
subbrep_is_sphere(struct subbrep_object_data *data, fastf_t cyl_tol)
{
    std::set<int>::iterator f_it;
    std::set<int> spherical_surfaces;
    // First, check surfaces.  If a surface is anything other than a sphere,
    // the verdict is no.
    for (int i = 0; i < data->faces_cnt; i++) {
	int f_ind = data->faces[i];
        int surface_type = (int)GetSurfaceType(data->brep->m_F[f_ind].SurfaceOf(), NULL);
        switch (surface_type) {
            case SURFACE_SPHERE:
            case SURFACE_SPHERICAL_SECTION:
                spherical_surfaces.insert(f_ind);
                break;
            default:
                return 0;
                break;
        }
    }

    // Second, check if all spherical surfaces share the same center and radius.
    ON_Sphere sph;
    ON_Surface *cs = data->brep->m_F[*spherical_surfaces.begin()].SurfaceOf()->Duplicate();
    cs->IsSphere(&sph, BREP_SPHERICAL_TOL);
    delete cs;
    for (f_it = spherical_surfaces.begin(); f_it != spherical_surfaces.end(); f_it++) {
	ON_Sphere f_sph;
	ON_Surface *fcs = data->brep->m_F[(*f_it)].SurfaceOf()->Duplicate();
	fcs->IsSphere(&sph, BREP_SPHERICAL_TOL);
	delete fcs;
	if (f_sph.Center().DistanceTo(sph.Center()) > BREP_SPHERICAL_TOL) return 0;
	if (!NEAR_ZERO(f_sph.Radius() - sph.Radius(), BREP_SPHERICAL_TOL)) return 0;
    }


    // Third, remove degenerate edge sets.
    std::set<int> active_edges;
    array_to_set(&active_edges, data->edges, data->edges_cnt);
    subbrep_remove_arc_degenerate_edges(data, &active_edges);

    // Fourth, check for any remaining edges.  For a true
    // sphere, they should all be gone.
    if (active_edges.size() > 0) return 0;


    data->type = SPHERE;
    struct csg_object_params * obj;
    BU_GET(obj, struct csg_object_params);

    data->params->bool_op= 'u';  // TODO - not always union
    data->params->origin[0] = sph.Center().x;
    data->params->origin[1] = sph.Center().y;
    data->params->origin[2] = sph.Center().z;
    data->params->radius = sph.Radius();

    return 1;
}

/* Return -1 if the sphere face is pointing in toward the center,
 * 1 if it is pointing out, and 0 if there is some other problem */
int
negative_sphere(struct subbrep_object_data *data, int face_index, double sph_tol) {
    const ON_Surface *surf = data->brep->m_F[face_index].SurfaceOf();
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

    if (NEAR_ZERO(dotp, 0.000001)) return 0;
    if (dotp < 0) return 1;
    return -1;
}


int
sphere_csg(struct subbrep_object_data *data, fastf_t sph_tol)
{
    std::set<int> planar_surfaces;
    std::set<int> spherical_surfaces;
    std::set<int>::iterator f_it;
    for (int i = 0; i < data->faces_cnt; i++) {
	int f_ind = data->faces[i];
        int surface_type = (int)GetSurfaceType(data->brep->m_F[f_ind].SurfaceOf(), NULL);
        switch (surface_type) {
            case SURFACE_PLANE:
                planar_surfaces.insert(f_ind);
                break;
            case SURFACE_CYLINDER:
                spherical_surfaces.insert(f_ind);
                break;
            default:
		std::cout << "what???\n";
                return 0;
                break;
        }
    }
    data->params->bool_op = 'u'; // Initialize to union

    // Check for multiple spheres.
    ON_Sphere sph;
    ON_Surface *cs = data->brep->m_F[*spherical_surfaces.begin()].SurfaceOf()->Duplicate();
    cs->IsSphere(&sph, BREP_SPHERICAL_TOL);
    delete cs;
    for (f_it = spherical_surfaces.begin(); f_it != spherical_surfaces.end(); f_it++) {
	ON_Sphere f_sph;
	ON_Surface *fcs = data->brep->m_F[(*f_it)].SurfaceOf()->Duplicate();
	fcs->IsSphere(&sph, BREP_SPHERICAL_TOL);
	delete fcs;
	if (f_sph.Center().DistanceTo(sph.Center()) > BREP_SPHERICAL_TOL) return 0;
	if (!NEAR_ZERO(f_sph.Radius() - sph.Radius(), BREP_SPHERICAL_TOL)) return 0;
    }

    // Remove degenerate edge sets.
    std::set<int> active_edges;
    array_to_set(&active_edges, data->edges, data->edges_cnt);
    subbrep_remove_arc_degenerate_edges(data, &active_edges);

    // If we've got linear edges, right now it's too complicated - TODO
    // TODO - check for linear edges in active set.

    // Make sure the vertices of the non-degenerate arcs are coplanar - can be
    // handled but for now too complicated

    // Characterize the planes of the non-degenerate non-linear edges.  This will tell us how many arbs
    // are subtracted from the sphere.  If none, we have a true sphere - each unique plane indicates an
    // arb subtraction.

}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
