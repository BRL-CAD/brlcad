#include "common.h"

#include <set>
#include <map>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "bg/polygon.h"
#include "bg/tri_ray.h"
#include "shape_recognition.h"

#define L2_OFFSET 4
#define L3_OFFSET 6

HIDDEN void
verts_assemble(ON_2dPointArray &on2dpts, int *index, std::map<int, int> *local_to_verts, struct subbrep_island_data *data, const ON_BrepLoop *b_loop, int dir, int verts_offset)
{
    const ON_Brep *brep = data->brep;
    ON_2dPointArray points2d;

    if (!data || !b_loop) return;

    // Get ignored vertices reported from shoal building - this is where they'll matter
    std::set<int> ignored_verts;
    array_to_set(&ignored_verts, data->null_verts, data->null_vert_cnt);

    /* Now the fun begins.  If we have an outer loop that isn't CCW or an inner loop
     * that isn't CW, we need to assemble our verts2d polygon backwards */
    if (brep->LoopDirection(brep->m_L[b_loop->m_loop_index]) != dir) {
	for (int ti = 0; ti < b_loop->m_ti.Count(); ti++) {
	    int ti_rev = b_loop->m_ti.Count() - 1 - ti;
	    const ON_BrepTrim *trim = &(brep->m_T[b_loop->m_ti[ti_rev]]);
	    const ON_BrepEdge *edge = &(brep->m_E[trim->m_ei]);
	    int vert_ind;
	    if (trim->m_bRev3d) {
		vert_ind = edge->Vertex(1)->m_vertex_index;
	    } else {
		vert_ind = edge->Vertex(0)->m_vertex_index;
	    }
	    if (ignored_verts.find(vert_ind) == ignored_verts.end()) {
		const ON_Curve *trim_curve = trim->TrimCurveOf();
		ON_2dPoint cp = trim_curve->PointAt(trim_curve->Domain().Min());
		on2dpts.Append(cp);
		(*local_to_verts)[ti+verts_offset] = vert_ind;
		if (index) index[ti] = ti+verts_offset;
	    }
	}
    } else {
	for (int ti = 0; ti < b_loop->m_ti.Count(); ti++) {
	    const ON_BrepTrim *trim = &(brep->m_T[b_loop->m_ti[ti]]);
	    const ON_BrepEdge *edge = &(brep->m_E[trim->m_ei]);
	    int vert_ind;
	    if (trim->m_bRev3d) {
		vert_ind = edge->Vertex(0)->m_vertex_index;
	    } else {
		vert_ind = edge->Vertex(1)->m_vertex_index;
	    }
	    if (ignored_verts.find(vert_ind) == ignored_verts.end()) {
		const ON_Curve *trim_curve = trim->TrimCurveOf();
		ON_2dPoint cp = trim_curve->PointAt(trim_curve->Domain().Max());
		on2dpts.Append(cp);
		(*local_to_verts)[ti+verts_offset] = vert_ind;
		if (index) index[ti] = ti+verts_offset;
	    }
	}
    }
}

int
subbrep_polygon_tri(struct bu_vls *msgs, struct subbrep_island_data *data, int *loops, int loop_cnt, int **ffaces)
{
    const ON_Brep *brep = data->brep;


    point_t *all_verts = (point_t *)bu_calloc(data->brep->m_V.Count(), sizeof(point_t), "bot verts");
    for (int vi = 0; vi < data->brep->m_V.Count(); vi++) {
        VMOVE(all_verts[vi], data->brep->m_V[vi].Point());
    }


    // Accumulate faces in a std::vector, since we don't know how many we're going to get
    std::vector<int> all_faces;

    int num_faces;
    int *faces;
    int face_error;
    const ON_BrepFace *b_face = NULL;
    ON_2dPointArray on2dpts;
    point2d_t *verts2d = NULL;

    /* The triangulation will use only the points in the temporary verts2d
     * array and return with faces indexed accordingly, so we need a map to
     * translate them back to our final vert array */
    std::map<int, int> local_to_verts;

    /* Sanity check */
    if (loop_cnt < 1 || !data || !all_verts || !loops || !ffaces) return 0;

    /* Only one loop is easier */
    if (loop_cnt == 1) {
	const ON_BrepLoop *b_loop = &(brep->m_L[loops[0]]);
	b_face = b_loop->Face();

	if (!ffaces) return 0;

	/* We need to build a 2D vertex list for the triangulation, and as long
	 * as we make sure the vertex mapping accounts for flipped trims in 3D,
	 * the point indices returned for the 2D triangulation will correspond
	 * to the correct 3D points without any additional work.  In essence,
	 * we use the fact that the BRep gives us a ready-made 2D point
	 * parameterization to same some work.*/

	if (b_loop->m_ti.Count() == 0) {
	    return 0;
	}


	verts_assemble(on2dpts, NULL, &local_to_verts, data, b_loop, 1, 0);

#if 0
	if (!bn_polygon_clockwise(b_loop->m_ti.Count(), verts2d)) {
	    bu_log("degenerate loop %d for face %d - no go\n", b_loop->m_loop_index, b_face->m_face_index);
	    //return 0;
	}
#endif

	/* The real work - triangulate the 2D polygon to find out triangles for
	 * this particular B-Rep face */
	verts2d = (point2d_t *)bu_calloc(on2dpts.Count(), sizeof(point2d_t), "bot verts");
	for (int i = 0; i < on2dpts.Count(); i++) {
	    V2MOVE(verts2d[i], on2dpts[i]);
	}
	face_error = bg_polygon_triangulate(&faces, &num_faces, NULL, NULL, (const point2d_t *)verts2d, b_loop->m_ti.Count(), EAR_CLIPPING);

    } else {

	/* We've got multiple loops - more complex setup since we need to define a polygon
	 * with holes */
	size_t poly_npts, nholes, total_pnts;
	int *poly;
	int **holes_array;
	size_t *holes_npts;
	const ON_BrepLoop *b_oloop = &(brep->m_L[loops[0]]);
	b_face = b_oloop->Face();

	/* Set up the "parent" polygon's index based definition */
	poly_npts = b_oloop->m_ti.Count();
	poly = (int *)bu_calloc(poly_npts, sizeof(int), "outer polygon array");

	/* Set up the holes */
	nholes = loop_cnt - 1;
	holes_npts = (size_t *)bu_calloc(nholes, sizeof(size_t), "hole size array");
	holes_array = (int **)bu_calloc(nholes, sizeof(int *), "holes array");

	/* Get a total point count and initialize the holes_npts array.  We
	 * don't need to account for double-counting introduced by hole elimination
	 * here, since we re-use the same points rather than adding new ones */
	total_pnts = poly_npts;
	for (int i = 1; i < loop_cnt; i++) {
	    const ON_BrepLoop *b_loop = &(brep->m_L[loops[i]]);
	    holes_npts[i-1] = b_loop->m_ti.Count();
	    total_pnts += holes_npts[i-1];
	}

	/* Use the loop definitions to assemble the final input arrays */
	verts_assemble(on2dpts, poly, &local_to_verts, data, b_oloop, 1, 0);
	int offset = on2dpts.Count();;
	for (int i = 1; i < loop_cnt; i++) {
	    const ON_BrepLoop *b_loop = &(brep->m_L[loops[i]]);
	    holes_array[i-1] = (int *)bu_calloc(b_loop->m_ti.Count(), sizeof(int), "hole array");
	    verts_assemble(on2dpts, holes_array[i-1], &local_to_verts, data, b_loop, -1, offset);
	    offset += b_loop->m_ti.Count();
	}

	/* The real work - triangulate the 2D polygon to find out triangles for
	 * this particular B-Rep face */
	verts2d = (point2d_t *)bu_calloc(on2dpts.Count(), sizeof(point2d_t), "bot verts");
	for (int i = 0; i < on2dpts.Count(); i++) {
	    V2MOVE(verts2d[i], on2dpts[i]);
	}
	face_error = bg_nested_polygon_triangulate(&faces, &num_faces, NULL, NULL, poly, poly_npts, (const int **)holes_array, holes_npts, nholes, (const point2d_t *)verts2d, total_pnts, EAR_CLIPPING);

	// We have the triangles now, so free up memory...
	for (int i = 1; i < loop_cnt; i++) {
	    bu_free(holes_array[i-1], "free hole array");
	}
	bu_free(holes_array, "free holes array cnts");
	bu_free(holes_npts, "free array holding hole array cnts");
	bu_free(poly, "free polygon index array");

    }


    if (face_error || !faces) {
	if (msgs) bu_vls_printf(msgs, "%*sbot build failed for face %d - no go\n", L3_OFFSET, " ", b_face->m_face_index);
	bu_free(verts2d, "free tmp 2d vertex array");
	return 0;
    } else {
	/* We aren't done with the reversing fun - if the face is reversed, we need
	 * to flip our triangles as well */
	if (b_face->m_bRev) {
	    for (int f_ind = 0; f_ind < num_faces; f_ind++) {
		int itmp = faces[f_ind * 3 + 2];
		faces[f_ind * 3 + 2] = faces[f_ind * 3 + 1];
		faces[f_ind * 3 + 1] = itmp;
	    }
	}
	/* Now that we have our faces, map them from the local vertex
	 * indices to the global ones and insert the faces into the
	 * all_faces array */
	for (int f_ind = 0; f_ind < num_faces*3; f_ind++) {
	    all_faces.push_back(local_to_verts[faces[f_ind]]);
	}
	bu_free(verts2d, "free tmp 2d vertex array");
    }

    /* Now we can build the final faces array */
    (*ffaces) = (int *)bu_calloc(num_faces * 3, sizeof(int), "final bot verts");
    for (int i = 0; i < num_faces*3; i++) {
	(*ffaces)[i] = all_faces[i];
    }

    return num_faces;
}

/*
 * To determine if a polyhedron is inside or outside, we do a ray-based
 * test.  We will assume that determining the positive/negative status of one
 * face is sufficient - i.e., we will not check for flipped normals on faces.
 *
 * 1.  Iterate over the loops, and construct polygons.  Flip as needed per
 *     reversed faces.
   2.  Triangulate those polygons, and accumulate the triangulations
   3.  Construct a ray with a point outside the bounding box and
 *     a point on one of the faces (try to make sure the face normal is not
 *     close to perpendicular relative to the constructed ray.)  Intersect this
 *     ray with all triangles in the BoT (unless BoTs regularly appear that are
 *     *far* larger than expected here, it's not worth building acceleration
 *     structures for a one time, one ray conversion process.
 * 4.  Count the intersections, and determine whether to test for an angle
 *     between the face normal and the ray of >ON_PI or <ON_PI.
 * 5.  Do the test as determined by #4.
 */

int
negative_polygon(struct bu_vls *msgs, struct subbrep_island_data *data)
{
    int io_state = 0;
    int all_faces_cnt = 0;
    std::vector<int> all_faces;
    int *final_faces = NULL;
    std::set<int> fol_faces;

    array_to_set(&fol_faces, data->fol, data->fol_cnt);


    point_t *all_verts = (point_t *)bu_calloc(data->brep->m_V.Count(), sizeof(point_t), "bot verts");
    for (int vi = 0; vi < data->brep->m_V.Count(); vi++) {
        VMOVE(all_verts[vi], data->brep->m_V[vi].Point());
    }

    // Check each face to see if it is fil or fol - the first fol face, stash its
    // normal - don't even need the triangle face normal, we can just use the face's normal and
    // a point from the center of one of the fol triangles on that particular face.
    ON_3dPoint origin_pnt;
    ON_3dVector triangle_normal;
    int have_hit_pnt = 0;

    /* Get triangles from the faces */
    ON_BoundingBox vert_bbox;
    ON_MinMaxInit(&vert_bbox.m_min, &vert_bbox.m_max);
    for (int i = 0; i < data->loops_cnt; i++) {
	const ON_BrepLoop *b_loop = &(data->brep->m_L[data->loops[i]]);
	int *ffaces = NULL;
	int num_faces = subbrep_polygon_tri(msgs, data, (int *)&(b_loop->m_loop_index), 1, &ffaces);
	if (!num_faces) {
	    if (msgs) bu_vls_printf(msgs, "%*sError - triangulation failed for loop %d!\n", L2_OFFSET, " ", b_loop->m_loop_index);
	    return 0;
	}
	if (!have_hit_pnt) {
	    const ON_BrepFace *b_face = b_loop->Face();
	    if (fol_faces.find(b_face->m_face_index) != fol_faces.end()) {
		ON_3dPoint p1 = data->brep->m_V[ffaces[0]].Point();
		ON_3dPoint p2 = data->brep->m_V[ffaces[1]].Point();
		ON_3dPoint p3 = data->brep->m_V[ffaces[2]].Point();
		ON_Plane fp;
		ON_Surface *ts = b_face->SurfaceOf()->Duplicate();
		(void)ts->IsPlanar(&fp, BREP_PLANAR_TOL);
		delete ts;
		triangle_normal = fp.Normal();
		if (b_face->m_bRev) triangle_normal = triangle_normal * -1;
		origin_pnt = (p1 + p2 + p3) / 3;
		have_hit_pnt = 1;
	    }
	}

	for (int f_ind = 0; f_ind < num_faces*3; f_ind++) {
	    all_faces.push_back(ffaces[f_ind]);
	    vert_bbox.Set(data->brep->m_V[ffaces[f_ind]].Point(), true);
	}
	if (ffaces) bu_free(ffaces, "free polygon face array");
	all_faces_cnt += num_faces;

    }

    /* Now we can build the final faces array */
    final_faces = (int *)bu_calloc(all_faces_cnt * 3, sizeof(int), "final bot verts");
    for (int i = 0; i < all_faces_cnt*3; i++) {
	final_faces[i] = all_faces[i];
    }

    // Scale bounding box to make sure corners are away from the volume
    vert_bbox.m_min = vert_bbox.m_min * 1.1;
    vert_bbox.m_max = vert_bbox.m_max * 1.1;

    // Pick a ray direction
    ON_3dVector rdir;
    ON_3dPoint box_corners[8];
    vert_bbox.GetCorners(box_corners);
    int have_dir = 0;
    int corner = 0;
    double dotp;
    while (!have_dir && corner < 8) {
	rdir = box_corners[corner] - origin_pnt;
	dotp = ON_DotProduct(triangle_normal, rdir);
	(NEAR_ZERO(dotp, 0.01)) ? corner++ : have_dir = 1;
    }
    if (!have_dir) {
	bu_log("Error: NONE of the corners worked??\n");
	return 0;
    }
    point_t origin, dir;
    VMOVE(origin, origin_pnt);
    VMOVE(dir, rdir);
#if 0
    std::cout << "working: " << bu_vls_addr(data->key) << "\n";
    bu_log("in origin.s sph %f %f %f 1\n", origin[0], origin[1], origin[2]);
    bu_log("in triangle_normal.s rcc %f %f %f %f %f %f 1 \n", origin_pnt.x, origin_pnt.y, origin_pnt.z, triangle_normal.x, triangle_normal.y, triangle_normal.z);
    bu_log("in ray.s rcc %f %f %f %f %f %f 1 \n", origin[0], origin[1], origin[2], dir[0], dir[1], dir[2]);
#endif
    // Test the ray against the triangle set
    int hit_cnt = 0;
    point_t p1, p2, p3, isect;
    ON_3dPointArray hit_pnts;
    for (int i = 0; i < all_faces_cnt; i++) {
	ON_3dPoint onp1, onp2, onp3, hit_pnt;
	VMOVE(p1, all_verts[all_faces[i*3+0]]);
	VMOVE(p2, all_verts[all_faces[i*3+1]]);
	VMOVE(p3, all_verts[all_faces[i*3+2]]);
	onp1.x = p1[0];
	onp1.y = p1[1];
	onp1.z = p1[2];
	onp2.x = p2[0];
	onp2.y = p2[1];
	onp2.z = p2[2];
	onp3.x = p3[0];
	onp3.y = p3[1];
	onp3.z = p3[2];
	ON_Plane fplane(onp1, onp2, onp3);
	int is_hit = bg_isect_tri_ray(origin, dir, p1, p2, p3, &isect);
	VMOVE(hit_pnt, isect);
	// Don't count the point on the ray origin
	if (hit_pnt.DistanceTo(origin_pnt) < 0.0001) is_hit = 0;
	if (is_hit) {
	    // No double-counting
	    for (int j = 0; j < hit_pnts.Count(); j++) {
		if (hit_pnts[j].DistanceTo(hit_pnt) < 0.001) is_hit = 0;
	    }
	    if (is_hit) {
		//bu_log("in hit_cnt%d.s sph %f %f %f 0.1\n", hit_pnts.Count()+1, isect[0], isect[1], isect[2]);
		hit_pnts.Append(hit_pnt);
	    }
	}
    }
    hit_cnt = hit_pnts.Count();
    //bu_log("hit count: %d\n", hit_cnt);
    //bu_log("dotp : %f\n", dotp);

    // Final inside/outside determination
    if (hit_cnt % 2) {
	io_state = (dotp > 0) ? -1 : 1;
    } else {
	io_state = (dotp < 0) ? -1 : 1;
    }

    //bu_log("inside out state: %d\n", io_state);

    bu_free(all_verts, "free top level vertex array");
    bu_free(final_faces, "free face array");
    return io_state;
}


int
island_nucleus(struct bu_vls *msgs, struct subbrep_island_data *data)
{
    // Accumulate per face triangle information
    std::vector< int * > face_arrays;
    std::vector< int > face_cnts;

    // Collect the set of all planes for convexity testing - if convex, we'll use an arbn
    // instead of a triangle mesh
    int is_convex = 1;
    ON_SimpleArray<ON_Plane> planes;

    // For this step, loops alone are not enough.  It's quite possible to have
    // an island containing a face with both inner and outer loops, so we need
    // the set of planar faces and for each face the set of loops in the
    // island.
    std::set<int> planar_faces;
    for (int i = 0; i < data->faces_cnt; i++) {
	const ON_BrepFace *face = &(data->brep->m_F[data->faces[i]]);
	surface_t surface_type = ((surface_t *)data->face_surface_types)[face->m_face_index];
	if (surface_type == SURFACE_PLANE) {
	    ON_Plane p;
	    face->SurfaceOf()->IsPlanar(&p, BREP_PLANAR_TOL);
	    planes.Append(p);
	    planar_faces.insert(face->m_face_index);
	}
    }

    // Get in-island loops
    std::set<int> island_loops;
    array_to_set(&island_loops, data->loops, data->loops_cnt);

    // First, deal with planar faces
    std::set<int>::iterator p_it;
    for (p_it = planar_faces.begin(); p_it != planar_faces.end(); p_it++) {
	int f_lcnt;
	int *f_loops;
	std::set<int> active_loops;
	const ON_BrepFace *face = &(data->brep->m_F[(int)*p_it]);
	// Determine if we have 1 or a set of loops.
	for (int i = 0; i < face->LoopCount(); i++) {
	    if (island_loops.find(face->m_li[i]) != island_loops.end()) active_loops.insert(face->m_li[i]);
	}
	if (f_lcnt > 1) is_convex = 0;
	set_to_array(&f_loops, &f_lcnt, &active_loops);
	int *faces;
	int face_cnt = subbrep_polygon_tri(msgs, data, f_loops, f_lcnt, &faces);
	if (face_cnt > 0) {
	    face_cnts.push_back(face_cnt);
	    face_arrays.push_back(faces);
	}
    }

    // Second, deal with non-planar shoals.


    // Third, generate compact vertex list for csg params
    // Fourth, map old vertex indices to new
    // Fifth, update face arrays to use the new indices

    // Sixth, test for a negative polyhedron - if negative, handle accordingly
    if (negative_polygon(msgs, data) == -1) {
	data->params->bool_op = '-';
	// Flip triangles so BoT/arbn is a positive shape
    }

    // Lastly, check to see if we have a convex polyhedron.  If so, define the arbn.
    if (is_convex) {
	//TODO: is_convex = is_convex(planes, verts);
	if (is_convex) {
	}
    }

    return 1;
}

int
subbrep_is_planar(struct bu_vls *msgs, struct subbrep_island_data *data)
{
    int i = 0;
    // Check surfaces.  If a surface is anything other than a plane the verdict is no.
    // If any face has more than one loop, the verdict is no.
    // If all surfaces are planes and the faces have only outer loops, then the verdict is yes.
    for (i = 0; i < data->faces_cnt; i++) {
	const ON_BrepFace *face = &(data->brep->m_F[i]);
	surface_t stype = ((surface_t *)data->face_surface_types)[face->m_face_index];
        if (stype != SURFACE_PLANE) return 0;
    }
    data->type = PLANAR_VOLUME;

    if (negative_polygon(msgs, data) == -1) data->params->bool_op = '-';

    return 1;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
