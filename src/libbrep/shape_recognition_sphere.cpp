#include "common.h"

#include <set>
#include <map>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "shape_recognition.h"

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

    // TODO - devise other tests necessary to make sure we have a closed sphere,
    // if any are needed.  Maybe rule out anything with linear edges?


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
            case SURFACE_SPHERE:
            case SURFACE_SPHERICAL_SECTION:
                spherical_surfaces.insert(f_ind);
                break;
            default:
		std::cout << "what???\n";
                return 0;
                break;
        }
    }
    data->params->bool_op = 'u'; // Initialize to union

    std::cout << "processing spherical surface\n";

    // Check for multiple spheres.
    ON_Sphere sph;
    ON_Surface *cs = data->brep->m_F[*spherical_surfaces.begin()].SurfaceOf()->Duplicate();
    cs->IsSphere(&sph, BREP_SPHERICAL_TOL);
    std::cout << "Center: " << pout(sph.Center()) << "\n";
    std::cout << "Radius: " << sph.Radius() << "\n";
    delete cs;
    for (f_it = spherical_surfaces.begin(); f_it != spherical_surfaces.end(); f_it++) {
	ON_Sphere f_sph;
	ON_Surface *fcs = data->brep->m_F[(*f_it)].SurfaceOf()->Duplicate();
	fcs->IsSphere(&f_sph, BREP_SPHERICAL_TOL);
	delete fcs;
	std::cout << "  Center: " << pout(f_sph.Center()) << "\n";
	std::cout << "  Radius: " << f_sph.Radius() << "\n";
	if (f_sph.Center().DistanceTo(sph.Center()) > BREP_SPHERICAL_TOL) return 0;
	if (!NEAR_ZERO(f_sph.Radius() - sph.Radius(), BREP_SPHERICAL_TOL)) return 0;
    }


    // Count the number of vertices associated with this subbrep.
    std::set<int> verts;
    std::set<int>::iterator v_it;
    for (int i = 0; i < data->edges_cnt; i++) {
	const ON_BrepEdge *edge = &(data->brep->m_E[data->edges[i]]);
	verts.insert(edge->Vertex(0)->m_vertex_index);
	verts.insert(edge->Vertex(1)->m_vertex_index);
    }

    std::cout << "vertex count: " << verts.size() << "\n";

    if (verts.size() == 1) {
	std::cout << "Only one vertex??\n";
	return 0;
    }

    if (verts.size() == 2 && data->edges_cnt == 1) {
	std::cout << "Two vertices, one edge - probably whole sphere\n";
	return 0;
    }

    if (verts.size() == 2 && data->edges_cnt >= 2) {
	std::cout << "Two vertices, more than one edge.  Uh...\n";
	return 0;
    }

    if (verts.size() == 3) {
	std::cout << "Three vertices\n";
	// Need the planes of any non-linear edges and the plane of the three verts.
	ON_SimpleArray<const ON_BrepVertex *> sph_verts(3);
	std::set<int> trims;
	for (v_it = verts.begin(); v_it != verts.end(); v_it++) {
	    sph_verts.Append(&(data->brep->m_V[*v_it]));
	}
	ON_3dPoint p1 = sph_verts[0]->Point();
	ON_3dPoint p2 = sph_verts[1]->Point();
	ON_3dPoint p3 = sph_verts[2]->Point();
	ON_Plane back_plane(p1, p2, p3);

	// In order to determine orientations, we need to know
	// what the normal is in an "interior" point of the surface (i.e.
	// within the trimming loop.)  Properly speaking we should
	// do a trimmed/untrimmed test on the candidate point to make
	// sure we've got a valid test point...
	const ON_BrepFace *sph_face = &(data->brep->m_F[(*spherical_surfaces.begin())]);
	const ON_BrepLoop *sph_face_loop = sph_face->OuterLoop();
	std::cout << "face " << sph_face->m_face_index << "\n";
	double u = 0;
	double v = 0;
	for (int i = 0; i < sph_face_loop->TrimCount(); i++) {
	    const ON_BrepTrim *trim = sph_face_loop->Trim(i);
	    const ON_Curve *trim_curve = trim->TrimCurveOf();
	    ON_3dPoint start = trim_curve->PointAtStart();
	    ON_3dPoint end = trim_curve->PointAtEnd();
	    u += start.x;
	    v += start.y;
	    u += end.x;
	    v += end.y;
	}
	u = u / (2 * sph_face_loop->TrimCount());
	v = v / (2 * sph_face_loop->TrimCount());

	std::cout << "u,v: " << u << "," << v << "\n";

	// TODO - do trimmed/not-trimmed test here.  Should probably devise a test that
	// doesn't require the curve tree, since this isn't a comprehensive
	// raytracing but just one or a few inside/outside tests.  We need an untrimmed
	// point.

	// Evaluate surface at point u,v to get both point and normal

	// Construct the vector between the sph point and the center, the vector between
	// the center and the closest point on a given plane (arc or
	// 3-pt based) and whether or not the sphere is negative will
	// have to tell us what we're dealing with.


	return 0;
    }

    if (verts.size() >= 3) {
	std::cout << "Complex situation.\n";
	return 0;
    }

}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
