#include "common.h"

#include <set>
#include <map>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "bn/polygon.h"
#include "bn/tri_ray.h"
#include "shape_recognition.h"


int
subbrep_polygon_tri(const ON_Brep *brep, const point_t *all_verts, int loop_ind, int **ffaces)
{
    // Accumulate faces in a std::vector, since we don't know how many we're going to get
    std::vector<int> all_faces;

    int num_faces;
    int *faces;

    const ON_BrepLoop *b_loop = &(brep->m_L[loop_ind]);
    const ON_BrepFace *b_face = b_loop->Face();

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

    point2d_t *verts2d = (point2d_t *)bu_calloc(b_loop->m_ti.Count(), sizeof(point2d_t), "bot verts");

    /* The triangulation will use only the points in the temporary verts2d
     * array and return with faces indexed accordingly, so we need a map to
     * translate them back to our final vert array */
    std::map<int, int> local_to_verts;

    /* Now the fun begins.  If we have an outer loop that isn't CCW, we
     * need to assemble our verts2d polygon backwards */
    if (brep->LoopDirection(brep->m_L[b_loop->m_loop_index]) != 1) {
	for (int ti = 0; ti < b_loop->m_ti.Count(); ti++) {
	    int ti_rev = b_loop->m_ti.Count() - 1 - ti;
	    const ON_BrepTrim *trim = &(b_face->Brep()->m_T[b_loop->m_ti[ti_rev]]);
	    const ON_BrepEdge *edge = &(b_face->Brep()->m_E[trim->m_ei]);
	    const ON_Curve *trim_curve = trim->TrimCurveOf();
	    ON_2dPoint cp = trim_curve->PointAt(trim_curve->Domain().Min());
	    ON_2dPoint cp_max = trim_curve->PointAt(trim_curve->Domain().Max());
	    V2MOVE(verts2d[ti], cp);
	    if (trim->m_bRev3d) {
		local_to_verts[ti] = edge->Vertex(1)->m_vertex_index;
	    } else {
		local_to_verts[ti] = edge->Vertex(0)->m_vertex_index;
	    }
	}
    } else {
	for (int ti = 0; ti < b_loop->m_ti.Count(); ti++) {
	    const ON_BrepTrim *trim = &(b_face->Brep()->m_T[b_loop->m_ti[ti]]);
	    const ON_BrepEdge *edge = &(b_face->Brep()->m_E[trim->m_ei]);
	    const ON_Curve *trim_curve = trim->TrimCurveOf();
	    ON_2dPoint cp = trim_curve->PointAt(trim_curve->Domain().Max());
	    V2MOVE(verts2d[ti], cp);
	    if (trim->m_bRev3d) {
		local_to_verts[ti] = edge->Vertex(0)->m_vertex_index;
	    } else {
		local_to_verts[ti] = edge->Vertex(1)->m_vertex_index;
	    }
	}
    }

#if 0
    if (!bn_polygon_clockwise(b_loop->m_ti.Count(), verts2d)) {
	bu_log("degenerate loop %d for face %d - no go\n", b_loop->m_loop_index, b_face->m_face_index);
	//return 0;
    }
#endif

    /* The real work - triangulate the 2D polygon to find out triangles for
     * this particular B-Rep face */
    int face_error = bn_polygon_triangulate(&faces, &num_faces, (const point2d_t *)verts2d, b_loop->m_ti.Count());
    if (face_error || !faces) {
	bu_log("bot build failed for face %d - no go\n", b_face->m_face_index);
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
negative_polygon(struct subbrep_object_data *data)
{
    int io_state = 0;
    int all_faces_cnt = 0;
    std::vector<int> all_faces;
    int *final_faces = NULL;
    std::set<int> fol_faces;

    /* This will get reused for all faces, so make it once */
    point_t *all_verts = (point_t *)bu_calloc(data->brep->m_V.Count(), sizeof(point_t), "bot verts");
    for (int vi = 0; vi < data->brep->m_V.Count(); vi++) {
        VMOVE(all_verts[vi], data->brep->m_V[vi].Point());
    }

    array_to_set(&fol_faces, data->fol, data->fol_cnt);

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
	int num_faces = subbrep_polygon_tri(data->brep, all_verts, b_loop->m_loop_index, &ffaces);
	if (!num_faces) {
	    bu_log("Error - triangulation failed for loop %d!\n", b_loop->m_loop_index);
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
    point2d_t pts2d[3];
    point2d_t test_pt;
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
	int is_hit = bn_isect_tri_ray(origin, dir, p1, p2, p3, &isect);
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
subbrep_is_planar(struct subbrep_object_data *data)
{
    int i = 0;
    // Check surfaces.  If a surface is anything other than a plane the verdict is no.
    // If all surfaces are planes, then the verdict is yes.
    for (i = 0; i < data->faces_cnt; i++) {
	surface_t stype = GetSurfaceType(data->brep->m_F[data->faces[i]].SurfaceOf(), NULL);
	if (stype != SURFACE_PLANE) return 0;
    }
    data->type = PLANAR_VOLUME;

    if (negative_polygon(data) == -1) data->params->bool_op = '-';

    return 1;
}

int
characterize_vert(struct subbrep_object_data *data, const ON_BrepVertex *v)
{
    int non_linear_edge_cnt = 0;
    int linear_edge_both_faces_nonplanar_cnt = 0;

    for (int i = 0; i < v->m_ei.Count(); i++) {
	std::set<int> efaces;
	std::set<int>::iterator f_it;
	const ON_BrepEdge *edge = &(data->brep->m_E[v->m_ei[i]]);
	ON_Curve *tc = edge->EdgeCurveOf()->Duplicate();
	if (tc->IsLinear()) {
	    // Get the faces associated with the edge
	    for (int j = 0; j < edge->TrimCount(); j++) {
		ON_BrepTrim *trim = edge->Trim(j);
		efaces.insert(trim->Face()->m_face_index);
	    }
	    int planar_face_cnt = 0;
	    for(f_it = efaces.begin(); f_it != efaces.end(); f_it++) {
		surface_t stype = GetSurfaceType(data->brep->m_F[*f_it].SurfaceOf(), NULL);
		if (stype == SURFACE_PLANE) planar_face_cnt++;
	    }
	    if (planar_face_cnt == 0) linear_edge_both_faces_nonplanar_cnt++;
	} else {
	    non_linear_edge_cnt++;
	}
    }

    if (non_linear_edge_cnt + linear_edge_both_faces_nonplanar_cnt == v->m_ei.Count()) return 1;

    return 0;
}

void
subbrep_planar_init(struct subbrep_object_data *data)
{
    if (!data) return;
    if (data->planar_obj) return;
    BU_GET(data->planar_obj, struct subbrep_object_data);
    subbrep_object_init(data->planar_obj, data->brep);
    bu_vls_sprintf(data->planar_obj->key, "%s", bu_vls_addr(data->key));
    data->planar_obj->obj_cnt = data->obj_cnt;
    (*data->obj_cnt)++;
    bu_vls_sprintf(data->planar_obj->name_root, "%s_%d", bu_vls_addr(data->name_root), *(data->obj_cnt));
    data->planar_obj->type = PLANAR_VOLUME;

    data->planar_obj->local_brep = ON_Brep::New();
    std::map<int, int> face_map;
    std::map<int, int> surface_map;
    std::map<int, int> edge_map;
    std::map<int, int> vertex_map;
    std::map<int, int> loop_map;
    std::map<int, int> c3_map;
    std::map<int, int> c2_map;
    std::map<int, int> trim_map;
    std::set<int> faces;
    std::set<int> fil;
    std::set<int> loops;
    std::set<int> skip_verts;
    std::set<int> skip_edges;
    std::set<int> keep_verts;
    std::set<int> partial_edges;
    std::set<int> isolated_trims;  // collect 2D trims whose parent loops aren't fully included here
    array_to_set(&faces, data->faces, data->faces_cnt);
    array_to_set(&fil, data->fil, data->fil_cnt);
    array_to_set(&loops, data->loops, data->loops_cnt);

    for (int i = 0; i < data->edges_cnt; i++) {
	int c3i;
	int new_edge_curve = 0;
	const ON_BrepEdge *old_edge = &(data->brep->m_E[data->edges[i]]);
	//std::cout << "old edge: " << old_edge->Vertex(0)->m_vertex_index << "," << old_edge->Vertex(1)->m_vertex_index << "\n";

	// See if the vertices from this edge play a role in the planar volume
	int use_edge = 2;
	for (int vi = 0; vi < 2; vi++) {
	    int vert_test = -1;
	    int vert_ind = old_edge->Vertex(vi)->m_vertex_index;
	    if (skip_verts.find(vert_ind) != skip_verts.end()) {
		vert_test = 1;
	    }
	    if (vert_test == -1 && keep_verts.find(vert_ind) != keep_verts.end()) {
		vert_test = 0;
	    }
	    if (vert_test == -1) {
		vert_test = characterize_vert(data, old_edge->Vertex(vi));
		if (vert_test) {
		    skip_verts.insert(vert_ind);
		    ON_3dPoint vp = old_edge->Vertex(vi)->Point();
		    bu_log("vert %d (%f %f %f): %d\n", vert_ind, vp.x, vp.y, vp.z, vert_test);
		} else {
		    keep_verts.insert(vert_ind);
		}
	    }
	    if (vert_test == 1) {
		use_edge--;
	    }
	}

	if (use_edge == 0) {
	    bu_log("skipping edge %d - both verts are skips\n", old_edge->m_edge_index);
	    skip_edges.insert(old_edge->m_edge_index);
	    continue;
	}

	if (use_edge == 1) {
	    bu_log("One of the verts for edge %d is a skip.\n", old_edge->m_edge_index);
	    partial_edges.insert(old_edge->m_edge_index);
	    continue;
	}

	// Get the 3D curves from the edges
	if (c3_map.find(old_edge->EdgeCurveIndexOf()) == c3_map.end()) {
	    ON_Curve *nc = old_edge->EdgeCurveOf()->Duplicate();
	    ON_Curve *tc = old_edge->EdgeCurveOf()->Duplicate();
	    if (tc->IsLinear()) {
		c3i = data->planar_obj->local_brep->AddEdgeCurve(nc);
		c3_map[old_edge->EdgeCurveIndexOf()] = c3i;
	    } else {
		ON_Curve *c3 = new ON_LineCurve(old_edge->Vertex(0)->Point(), old_edge->Vertex(1)->Point());
		c3i = data->planar_obj->local_brep->AddEdgeCurve(c3);
		c3_map[old_edge->EdgeCurveIndexOf()] = c3i;
		new_edge_curve = 1;
	    }
	} else {
	    c3i = c3_map[old_edge->EdgeCurveIndexOf()];
	}


	// Get the vertices from the edges
	int v[2];
	for (int vi = 0; vi < 2; vi++) {
	    if (vertex_map.find(old_edge->Vertex(vi)->m_vertex_index) == vertex_map.end()) {
		ON_BrepVertex& newvvi = data->planar_obj->local_brep->NewVertex(old_edge->Vertex(vi)->Point(), old_edge->Vertex(vi)->m_tolerance);
		v[vi] = newvvi.m_vertex_index;
		vertex_map[old_edge->Vertex(vi)->m_vertex_index] = v[vi];
	    } else {
		v[vi] = vertex_map[old_edge->Vertex(vi)->m_vertex_index];
	    }
	    ON_3dPoint vp = old_edge->Vertex(vi)->Point();
	}

	ON_BrepEdge& new_edge = data->planar_obj->local_brep->NewEdge(data->planar_obj->local_brep->m_V[v[0]], data->planar_obj->local_brep->m_V[v[1]], c3i, NULL ,0);
	edge_map[old_edge->m_edge_index] = new_edge.m_edge_index;

	// Get the 2D curves from the trims
	for (int j = 0; j < old_edge->TrimCount(); j++) {
	    ON_BrepTrim *old_trim = old_edge->Trim(j);
	    if (faces.find(old_trim->Face()->m_face_index) != faces.end()) {
		if (c2_map.find(old_trim->TrimCurveIndexOf()) == c2_map.end()) {
		    ON_Curve *nc = old_trim->TrimCurveOf()->Duplicate();
		    int c2i = data->planar_obj->local_brep->AddTrimCurve(nc);
		    c2_map[old_trim->TrimCurveIndexOf()] = c2i;
		    //std::cout << "c2i: " << c2i << "\n";
		}
	    }
	}

	// Get the faces and surfaces from the trims
	for (int j = 0; j < old_edge->TrimCount(); j++) {
	    ON_BrepTrim *old_trim = old_edge->Trim(j);
	    if (face_map.find(old_trim->Face()->m_face_index) == face_map.end()) {
		if (faces.find(old_trim->Face()->m_face_index) != faces.end()) {
		    ON_Surface *ns = old_trim->Face()->SurfaceOf()->Duplicate();
		    ON_Surface *ts = old_trim->Face()->SurfaceOf()->Duplicate();
		    if (ts->IsPlanar(NULL, BREP_PLANAR_TOL)) {
			int nsid = data->planar_obj->local_brep->AddSurface(ns);
			surface_map[old_trim->Face()->SurfaceIndexOf()] = nsid;
			ON_BrepFace &new_face = data->planar_obj->local_brep->NewFace(nsid);
			face_map[old_trim->Face()->m_face_index] = new_face.m_face_index;
			//std::cout << "old face " << old_trim->Face()->m_face_index << " is now " << new_face.m_face_index << "\n";
			if (fil.find(old_trim->Face()->m_face_index) != fil.end()) {
			    data->planar_obj->local_brep->FlipFace(new_face);
			}
		    }
		}
	    }
	}

	// Get the loops from the trims
	for (int j = 0; j < old_edge->TrimCount(); j++) {
	    ON_BrepTrim *old_trim = old_edge->Trim(j);
	    ON_BrepLoop *old_loop = old_trim->Loop();
	    if (face_map.find(old_trim->Face()->m_face_index) != face_map.end()) {
		if (loops.find(old_loop->m_loop_index) != loops.end()) {
		    if (loop_map.find(old_loop->m_loop_index) == loop_map.end()) {
			// After the initial breakout, all loops in any given subbrep are outer loops,
			// whatever they were in the original brep.
			ON_BrepLoop &nl = data->planar_obj->local_brep->NewLoop(ON_BrepLoop::outer, data->planar_obj->local_brep->m_F[face_map[old_loop->m_fi]]);
			loop_map[old_loop->m_loop_index] = nl.m_loop_index;
		    }
		}
	    }
	}
    }

    // Now, create new trims using the old loop definitions and the maps
    std::map<int, int>::iterator loop_it;
    std::set<int> evaluated;
    for (loop_it = loop_map.begin(); loop_it != loop_map.end(); loop_it++) {
	const ON_BrepLoop *old_loop = &(data->brep->m_L[(*loop_it).first]);
	ON_BrepLoop &new_loop = data->planar_obj->local_brep->m_L[(*loop_it).second];
	for (int j = 0; j < old_loop->TrimCount(); j++) {
	    const ON_BrepTrim *old_trim = old_loop->Trim(j);
	    ON_BrepEdge *o_edge = old_trim->Edge();
	    if (!o_edge) {
		/* If we didn't have an edge originally, we need to add the 2d curve here */
		if (c2_map.find(old_trim->TrimCurveIndexOf()) == c2_map.end()) {
		    ON_Curve *nc = old_trim->TrimCurveOf()->Duplicate();
		    int c2i = data->planar_obj->local_brep->AddTrimCurve(nc);
		    c2_map[old_trim->TrimCurveIndexOf()] = c2i;
		}
		if (vertex_map.find(old_trim->Vertex(0)->m_vertex_index) == vertex_map.end()) {
		    ON_BrepVertex& newvs = data->planar_obj->local_brep->NewVertex(old_trim->Vertex(0)->Point(), old_trim->Vertex(0)->m_tolerance);
		    vertex_map[old_trim->Vertex(0)->m_vertex_index] = newvs.m_vertex_index;
		    ON_BrepTrim &nt = data->planar_obj->local_brep->NewSingularTrim(newvs, new_loop, old_trim->m_iso, c2_map[old_trim->TrimCurveIndexOf()]);
		    nt.m_tolerance[0] = old_trim->m_tolerance[0];
		    nt.m_tolerance[1] = old_trim->m_tolerance[1];
		} else {
		    ON_BrepTrim &nt = data->planar_obj->local_brep->NewSingularTrim(data->planar_obj->local_brep->m_V[vertex_map[old_trim->Vertex(0)->m_vertex_index]], new_loop, old_trim->m_iso, c2_map[old_trim->TrimCurveIndexOf()]);
		    nt.m_tolerance[0] = old_trim->m_tolerance[0];
		    nt.m_tolerance[1] = old_trim->m_tolerance[1];
		}
		continue;
	    }

	    if (evaluated.find(o_edge->m_edge_index) != evaluated.end()) {
		bu_log("edge %d already handled, continuing...\n", o_edge->m_edge_index);
		continue;
	    }

	    // Don't use a trim connected to an edge we are skipping
	    if (skip_edges.find(o_edge->m_edge_index) != skip_edges.end()) {
		bu_log("edge %d is skipped, continuing...\n", o_edge->m_edge_index);
		evaluated.insert(o_edge->m_edge_index);
		continue;
	    }

	    int is_partial = 0;
	    if (partial_edges.find(o_edge->m_edge_index) != partial_edges.end()) is_partial = 1;

	    if (!is_partial) {
		ON_BrepEdge &n_edge = data->planar_obj->local_brep->m_E[edge_map[o_edge->m_edge_index]];
		ON_Curve *ec = o_edge->EdgeCurveOf()->Duplicate();
		if (ec->IsLinear()) {
		    ON_BrepTrim &nt = data->planar_obj->local_brep->NewTrim(n_edge, old_trim->m_bRev3d, new_loop, c2_map[old_trim->TrimCurveIndexOf()]);
		    nt.m_tolerance[0] = old_trim->m_tolerance[0];
		    nt.m_tolerance[1] = old_trim->m_tolerance[1];
		    nt.m_iso = old_trim->m_iso;
		} else {
		    // Wasn't linear, but wasn't partial either - replace with a line
		    ON_Curve *c2_orig = old_trim->TrimCurveOf()->Duplicate();
		    ON_3dPoint p1 = c2_orig->PointAt(c2_orig->Domain().Min());
		    ON_3dPoint p2 = c2_orig->PointAt(c2_orig->Domain().Max());
		    ON_Curve *c2 = new ON_LineCurve(p1, p2);
		    c2->ChangeDimension(2);
		    int c2i = data->planar_obj->local_brep->AddTrimCurve(c2);
		    ON_BrepTrim &nt = data->planar_obj->local_brep->NewTrim(n_edge, old_trim->m_bRev3d, new_loop, c2i);
		    nt.m_tolerance[0] = old_trim->m_tolerance[0];
		    nt.m_tolerance[1] = old_trim->m_tolerance[1];
		    nt.m_iso = old_trim->m_iso;
		    delete c2_orig;
		}
		delete ec;
	    } else {
		// Partial edge - let the fun begin
		ON_3dPoint p1, p2;
		ON_BrepEdge *next_edge;
		bu_log("working a partial edge: %d\n", o_edge->m_edge_index);
		int v[2];
		v[0] = o_edge->Vertex(0)->m_vertex_index;
		v[1] = o_edge->Vertex(1)->m_vertex_index;
		// figure out which trim point we can use, the min or max
		int pos1 = 0;
		if (skip_verts.find(v[0]) != skip_verts.end()) {
		    pos1 = 1;
		}
		int j_next = j;
		ON_Curve *c2_orig = old_trim->TrimCurveOf()->Duplicate();
		ON_Curve *c2_next = NULL;
		int walk_dir = 1;
		// bump the loop iterator to get passed any skipped edges to
		// the next partial
		while (!c2_next) {
		    (walk_dir == 1) ? j_next++ : j_next--;
		    if (j_next == old_loop->TrimCount() && walk_dir == 1) {
			j_next = 0;
		    }
		    if (j_next == 0 && walk_dir == -1) {
			j_next = old_loop->TrimCount() - 1;
		    }
		    const ON_BrepTrim *next_trim = old_loop->Trim(j_next);
		    next_edge = next_trim->Edge();
		    if (skip_edges.find(next_edge->m_edge_index) == skip_edges.end()) {
			if (partial_edges.find(next_edge->m_edge_index) != partial_edges.end()) {
			    bu_log("found next partial edge %d\n", next_edge->m_edge_index);
			    evaluated.insert(next_edge->m_edge_index);
			    c2_next = next_trim->TrimCurveOf()->Duplicate();
			} else {
			    bu_log("partial edge %d followed by non-partial %d, need to go the other way\n", o_edge->m_edge_index, next_edge->m_edge_index);
			    j_next--;
			    walk_dir = -1;
			}
		    } else {
			bu_log("skipping fully ignored edge %d\n", next_edge->m_edge_index);
			evaluated.insert(next_edge->m_edge_index);
		    }
		}
		int v2[2];
		v2[0] = next_edge->Vertex(0)->m_vertex_index;
		v2[1] = next_edge->Vertex(1)->m_vertex_index;
		// figure out which trim point we can use, the min or max
		int pos2 = 0;
		if (skip_verts.find(v2[0]) != skip_verts.end()) {
		    pos2 = 1;
		}

		int vmapped[2];
		if (vertex_map.find(o_edge->Vertex(pos1)->m_vertex_index) == vertex_map.end()) {
		    ON_BrepVertex& newvvi = data->planar_obj->local_brep->NewVertex(o_edge->Vertex(pos1)->Point(), o_edge->Vertex(pos1)->m_tolerance);
		    vertex_map[o_edge->Vertex(pos1)->m_vertex_index] = newvvi.m_vertex_index;
		}
		if (vertex_map.find(next_edge->Vertex(pos2)->m_vertex_index) == vertex_map.end()) {
		    ON_BrepVertex& newvvi = data->planar_obj->local_brep->NewVertex(next_edge->Vertex(pos2)->Point(), next_edge->Vertex(pos2)->m_tolerance);
		    vertex_map[next_edge->Vertex(pos2)->m_vertex_index] = newvvi.m_vertex_index;
		}

		// If walk_dir is -1, need to flip things around (I think...) the verts and trim points
		// will be swapped compared to a forward walk
		if (walk_dir == -1) {
		    vmapped[1] = vertex_map[o_edge->Vertex(pos1)->m_vertex_index];
		    vmapped[0] = vertex_map[next_edge->Vertex(pos2)->m_vertex_index];
		} else {
		    vmapped[0] = vertex_map[o_edge->Vertex(pos1)->m_vertex_index];
		    vmapped[1] = vertex_map[next_edge->Vertex(pos2)->m_vertex_index];
		}

		// New Edge curve
		ON_Curve *c3 = new ON_LineCurve(o_edge->Vertex(pos1)->Point(), next_edge->Vertex(pos2)->Point());
		int c3i = data->planar_obj->local_brep->AddEdgeCurve(c3);
		ON_BrepEdge& new_edge = data->planar_obj->local_brep->NewEdge(data->planar_obj->local_brep->m_V[vmapped[0]], data->planar_obj->local_brep->m_V[vmapped[1]], c3i, NULL ,0);

		// Again, flip if walk_dir is -1
		if (walk_dir == -1) {
		    p2 = c2_orig->PointAt(c2_orig->Domain().Min());
		    p1 = c2_next->PointAt(c2_orig->Domain().Max());
		} else {
		    p1 = c2_orig->PointAt(c2_orig->Domain().Min());
		    p2 = c2_next->PointAt(c2_orig->Domain().Max());
		}
		std::cout << "p1: " << pout(p1) << "\n";
		std::cout << "p2: " << pout(p2) << "\n";
		ON_Curve *c2 = new ON_LineCurve(p1, p2);
		c2->ChangeDimension(2);
		int c2i = data->planar_obj->local_brep->AddTrimCurve(c2);
		ON_BrepTrim &nt = data->planar_obj->local_brep->NewTrim(new_edge, false, new_loop, c2i);
		nt.m_tolerance[0] = old_trim->m_tolerance[0];
		nt.m_tolerance[1] = old_trim->m_tolerance[1];
		nt.m_iso = old_trim->m_iso;
		delete c2_orig;
		delete c2_next;
	    }
	}
    }

    // Need to preserve the vertex map for this, since we're not done building up the brep
    map_to_array(&(data->planar_obj->planar_obj_vert_map), &(data->planar_obj->planar_obj_vert_cnt), &vertex_map);

    data->planar_obj->local_brep->SetTrimTypeFlags(true);

}

bool
end_of_trims_match(ON_Brep *brep, ON_BrepLoop *loop, int lti)
{
    bool valid = true;
    int ci0, ci1, next_lti;
    ON_3dPoint P0, P1;
    const ON_Curve *pC0, *pC1;
    const ON_Surface *surf = loop->Face()->SurfaceOf();
    double urange = surf->Domain(0)[1] - surf->Domain(0)[0];
    double vrange = surf->Domain(1)[1] - surf->Domain(1)[0];
    // end-of-trims matching test from opennurbs_brep.cpp
    const ON_BrepTrim& trim0 = brep->m_T[loop->m_ti[lti]];
    next_lti = (lti+1)%loop->TrimCount();
    const ON_BrepTrim& trim1 = brep->m_T[loop->m_ti[next_lti]];
    ON_Interval trim0_domain = trim0.Domain();
    ON_Interval trim1_domain = trim1.Domain();
    ci0 = trim0.m_c2i;
    ci1 = trim1.m_c2i;
    pC0 = brep->m_C2[ci0];
    pC1 = brep->m_C2[ci1];
    P0 = pC0->PointAt( trim0_domain[1] ); // end of this 2d trim
    P1 = pC1->PointAt( trim1_domain[0] ); // start of next 2d trim
    if ( !(P0-P1).IsTiny() )
    {
	// 16 September 2003 Dale Lear - RR 11319
	//    Added relative tol check so cases with huge
	//    coordinate values that agreed to 10 places
	//    didn't get flagged as bad.
	//double xtol = (fabs(P0.x) + fabs(P1.x))*1.0e-10;
	//double ytol = (fabs(P0.y) + fabs(P1.y))*1.0e-10;
	//
	// Oct 12 2009 Rather than using the above check, BRL-CAD uses
	// relative uv size
	double xtol = (urange) * trim0.m_tolerance[0];
	double ytol = (vrange) * trim0.m_tolerance[1];
	if ( xtol < ON_ZERO_TOLERANCE )
	    xtol = ON_ZERO_TOLERANCE;
	if ( ytol < ON_ZERO_TOLERANCE )
	    ytol = ON_ZERO_TOLERANCE;
	double dx = fabs(P0.x-P1.x);
	double dy = fabs(P0.y-P1.y);
	if ( dx > xtol || dy > ytol ) valid = false;
    }
    return valid;
}

void
subbrep_planar_close_obj(struct subbrep_object_data *data)
{
    struct subbrep_object_data *pdata = data->planar_obj;

    pdata->local_brep->Compact();
    pdata->local_brep->SetTrimIsoFlags();
    pdata->local_brep->SetTrimTypeFlags(true);

    pdata->brep = pdata->local_brep;
    /* By this point, if it's not already determined to
     * be negative, the volume is positive */
    if (pdata->params->bool_op != '-') pdata->params->bool_op = 'u';
}

void
subbrep_add_planar_face(struct subbrep_object_data *data, ON_Plane *pcyl,
			ON_SimpleArray<const ON_BrepVertex *> *vert_loop, int neg_surf)
{
    // We use the planar_obj's local_brep to store new faces.  The planar local
    // brep contains the relevant linear and planar components from its parent
    // - our job here is to add the new surface, identify missing edges to
    // create, find existing edges to re-use, and call NewFace with the
    // results.  At the end we should have just the faces needed
    // to define the planar volume of interest.
    struct subbrep_object_data *pdata = data->planar_obj;
    std::vector<int> edges;
    ON_SimpleArray<ON_Curve *> curves_2d;
    ON_SimpleArray<bool> reversed;
    std::map<int, int> vert_map;
    array_to_map(&vert_map, pdata->planar_obj_vert_map, pdata->planar_obj_vert_cnt);

    ON_3dPoint p1 = pdata->local_brep->m_V[vert_map[((*vert_loop)[0])->m_vertex_index]].Point();
    ON_3dPoint p2 = pdata->local_brep->m_V[vert_map[((*vert_loop)[1])->m_vertex_index]].Point();
    ON_3dPoint p3 = pdata->local_brep->m_V[vert_map[((*vert_loop)[2])->m_vertex_index]].Point();
    ON_Plane loop_plane(p1, p2, p3);
    ON_BoundingBox loop_pbox, cbox;

    // get 2d trim curves
    ON_Xform proj_to_plane;
    proj_to_plane[0][0] = loop_plane.xaxis.x;
    proj_to_plane[0][1] = loop_plane.xaxis.y;
    proj_to_plane[0][2] = loop_plane.xaxis.z;
    proj_to_plane[0][3] = -(loop_plane.xaxis*loop_plane.origin);
    proj_to_plane[1][0] = loop_plane.yaxis.x;
    proj_to_plane[1][1] = loop_plane.yaxis.y;
    proj_to_plane[1][2] = loop_plane.yaxis.z;
    proj_to_plane[1][3] = -(loop_plane.yaxis*loop_plane.origin);
    proj_to_plane[2][0] = loop_plane.zaxis.x;
    proj_to_plane[2][1] = loop_plane.zaxis.y;
    proj_to_plane[2][2] = loop_plane.zaxis.z;
    proj_to_plane[2][3] = -(loop_plane.zaxis*loop_plane.origin);
    proj_to_plane[3][0] = 0.0;
    proj_to_plane[3][1] = 0.0;
    proj_to_plane[3][2] = 0.0;
    proj_to_plane[3][3] = 1.0;

    ON_PlaneSurface *s = new ON_PlaneSurface(loop_plane);
    const int si = pdata->local_brep->AddSurface(s);

    double flip = ON_DotProduct(loop_plane.Normal(), pcyl->Normal()) * neg_surf;

    for (int i = 0; i < vert_loop->Count(); i++) {
	int vind1, vind2;
	const ON_BrepVertex *v1, *v2;
	v1 = (*vert_loop)[i];
	vind1 = vert_map[v1->m_vertex_index];
	if (i < vert_loop->Count() - 1) {
	    v2 = (*vert_loop)[i+1];
	} else {
	    v2 = (*vert_loop)[0];
	}
	vind2 = vert_map[v2->m_vertex_index];
	ON_BrepVertex &new_v1 = pdata->local_brep->m_V[vind1];
	ON_BrepVertex &new_v2 = pdata->local_brep->m_V[vind2];
	ON_3dPoint np1 = new_v1.Point();
	ON_3dPoint np2 = new_v2.Point();

	// Because we may have already created a needed edge only in the new
	// Brep with a previous face, we have to check all the edges in the new
	// structure for a vertex match.
	int edge_found = 0;
	for (int j = 0; j < pdata->local_brep->m_E.Count(); j++) {
	    int ev1 = pdata->local_brep->m_E[j].Vertex(0)->m_vertex_index;
	    int ev2 = pdata->local_brep->m_E[j].Vertex(1)->m_vertex_index;

	    ON_3dPoint pv1 = pdata->local_brep->m_E[j].Vertex(0)->Point();
	    ON_3dPoint pv2 = pdata->local_brep->m_E[j].Vertex(1)->Point();

	    if ((ev1 == vind1) && (ev2 == vind2)) {
		edges.push_back(pdata->local_brep->m_E[j].m_edge_index);
		edge_found = 1;

		reversed.Append(false);

		// Get 2D curve from this edge's 3D curve
		const ON_Curve *c3 = pdata->local_brep->m_E[j].EdgeCurveOf();
		ON_NurbsCurve *c2 = new ON_NurbsCurve();
		c3->GetNurbForm(*c2);
		c2->Transform(proj_to_plane);
		c2->GetBoundingBox(cbox);
		c2->ChangeDimension(2);
		c2->MakePiecewiseBezier(2);
		curves_2d.Append(c2);
		loop_pbox.Union(cbox);
		break;
	    }
	    if ((ev2 == vind1) && (ev1 == vind2)) {
		edges.push_back(pdata->local_brep->m_E[j].m_edge_index);
		edge_found = 1;
		reversed.Append(true);

		// Get 2D curve from this edge's points
		ON_Curve *c3 = new ON_LineCurve(pv2, pv1);
		ON_NurbsCurve *c2 = new ON_NurbsCurve();
		c3->GetNurbForm(*c2);
		c2->Transform(proj_to_plane);
		c2->GetBoundingBox(cbox);
		c2->ChangeDimension(2);
		c2->MakePiecewiseBezier(2);
		curves_2d.Append(c2);
		loop_pbox.Union(cbox);
		break;
	    }
	}
	if (!edge_found) {
	    int c3i = pdata->local_brep->AddEdgeCurve(new ON_LineCurve(new_v1.Point(), new_v2.Point()));
	    // Get 2D curve from this edge's 3D curve
	    const ON_Curve *c3 = pdata->local_brep->m_C3[c3i];
	    ON_NurbsCurve *c2 = new ON_NurbsCurve();
	    c3->GetNurbForm(*c2);
	    c2->Transform(proj_to_plane);
	    c2->GetBoundingBox(cbox);
	    c2->ChangeDimension(2);
	    c2->MakePiecewiseBezier(2);
	    curves_2d.Append(c2);
	    loop_pbox.Union(cbox);

	    ON_BrepEdge &new_edge = pdata->local_brep->NewEdge(pdata->local_brep->m_V[vind1], pdata->local_brep->m_V[vind2], c3i, NULL ,0);
	    edges.push_back(new_edge.m_edge_index);
	}
    }

    ON_BrepFace& face = pdata->local_brep->NewFace( si );
    ON_BrepLoop& loop = pdata->local_brep->NewLoop(ON_BrepLoop::outer, face);
    loop.m_pbox = loop_pbox;
    for (int i = 0; i < vert_loop->Count(); i++) {
	ON_NurbsCurve *c2 = (ON_NurbsCurve *)curves_2d[i];
	int c2i = pdata->local_brep->AddTrimCurve(c2);
	ON_BrepEdge &edge = pdata->local_brep->m_E[edges.at(i)];
	ON_BrepTrim &trim = pdata->local_brep->NewTrim(edge, reversed[i], loop, c2i);
	trim.m_type = ON_BrepTrim::mated;
	trim.m_tolerance[0] = 0.0;
	trim.m_tolerance[1] = 0.0;
    }

    // set face domain
    s->SetDomain(0, loop.m_pbox.m_min.x, loop.m_pbox.m_max.x );
    s->SetDomain(1, loop.m_pbox.m_min.y, loop.m_pbox.m_max.y );
    s->SetExtents(0,s->Domain(0));
    s->SetExtents(1,s->Domain(1));

    // need to update trim m_iso flags because we changed surface shape
    if (flip < 0) pdata->local_brep->FlipFace(face);
    pdata->local_brep->SetTrimIsoFlags(face);
    pdata->local_brep->SetTrimTypeFlags(true);
}




// TODO - need a way to detect if a set of planar faces would form a
// self-intersecting polyhedron.  Self-intersecting polyhedrons are the ones
// with the potential to make both positive and negative contributions to a
// solid. One possible idea:
//
// For all edges in polyhedron test whether each edge intersects any face in
// the polyhedron to which it does not belong.  The test can be simple - for
// each vertex, construct a vector from the vertex to some point on the
// candidate face (center point is probably a good start) and see if the dot
// products of those two vectors with the face normal vector agree in sign.
// For a given edge, if all dot products agree in pair sets, then the edge does
// not intersect any face in the polyhedron.  If no edges intersect, the
// polyhedron is not self intersecting.  If some edges do intersect (probably
// at least three...) then those edges identify sub-shapes that need to be
// constructed.
//
// Note that this is also a concern for non-planar surfaces that have been
// reduced to planar surfaces as part of the process - probably should
// incorporate a bounding box test to determine if such surfaces can be
// part of sub-object definitions (say, a cone subtracted from a subtracting
// cylinder to make a positive cone volume inside the cylinder) or if the
// cone has to be a top level unioned positive object (if the cone's apex
// point is outside the cylinder's subtracting area, for example, the tip
// of the code will not be properly added as a positive volume if it is just
// a subtraction from the cylinder.


// Returns 1 if point set forms a convex polyhedron, 0 if the point set
// forms a degenerate chull, and -1 if the point set is concave
    int
convex_point_set(struct subbrep_object_data *data, std::set<int> *verts)
{
    // Use chull3d to find the set of vertices that are on the convex
    // hull.  If all of them are, the point set defines a convex polyhedron.
    // If the points are coplanar, return 0 - not a volume
    return 0;
}

/* These functions will return 2 if successful, 1 if unsuccessful but point set
 * is convex, 0 if unsuccessful and vertex set's chull is degenerate (i.e. the
 * planar component of this CSG shape contributes no positive volume),  and -1
 * if unsuccessful and point set is neither degenerate nor convex */
    int
point_set_is_arb4(struct subbrep_object_data *data, std::set<int> *verts)
{
    int is_convex = convex_point_set(data, verts);
    if (is_convex == 1) {
	// TODO - deduce and set up proper arb4 point ordering
	return 2;
    }
    return 0;
}
    int
point_set_is_arb5(struct subbrep_object_data *data, std::set<int> *verts)
{
    int is_convex = convex_point_set(data, verts);

    if (!is_convex == 1) {
	return is_convex;
    }
    // TODO - arb5 test
    return 0;
}
    int
point_set_is_arb6(struct subbrep_object_data *data, std::set<int> *verts)
{
    int is_convex = convex_point_set(data, verts);

    if (!is_convex == 1) {
	return is_convex;
    }
    // TODO - arb6 test
    return 0;
}
    int
point_set_is_arb7(struct subbrep_object_data *data, std::set<int> *verts)
{
    int is_convex = convex_point_set(data, verts);

    if (!is_convex == 1) {
	return is_convex;
    }
    // TODO - arb7 test
    return 0;
}
    int
point_set_is_arb8(struct subbrep_object_data *data, std::set<int> *verts)
{
    int is_convex = convex_point_set(data, verts);

    if (!is_convex == 1) {
	return is_convex;
    }
    // TODO - arb8 test
    return 0;
}

/* If we're going with an arbn, we need to add one plane for each face.  To
 * make sure the normal is in the correct direction, find the center point of
 * the verts and the center point of the face verts to construct a vector which
 * can be used in a dot product test with the face normal.*/
    int
point_set_is_arbn(struct subbrep_object_data *data, std::set<int> *faces, std::set<int> *verts, int do_test)
{
    int is_convex;
    if (!do_test) {
	is_convex = 1;
    } else {
	is_convex = convex_point_set(data, verts);
    }
    if (!is_convex == 1) return is_convex;

    // TODO - arbn assembly
    return 2;
}

// In the worst case, make a brep for later conversion into an nmg.
// The other possibility here is an arbn csg tree, but that needs
// more thought...
    int
subbrep_make_planar_brep(struct subbrep_object_data *data)
{
    // TODO - check for self intersections in the candidate shape, and handle
    // if any are discovered.
    return 0;
}

    int
planar_switch(int ret, struct subbrep_object_data *data, std::set<int> *faces, std::set<int> *verts)
{
    switch (ret) {
	case -1:
	    return subbrep_make_planar_brep(data);
	    break;
	case 0:
	    return 0;
	    break;
	case 1:
	    return point_set_is_arbn(data, faces, verts, 0);
	    break;
	case 2:
	    return 1;
	    break;
    }
    return 0;
}


int
subbrep_make_planar(struct subbrep_object_data *data)
{
    // First step is to count vertices, using the edges
    std::set<int> subbrep_verts;
    std::set<int> faces;
    for (int i = 0; i < data->edges_cnt; i++) {
	const ON_BrepEdge *edge = &(data->brep->m_E[data->edges[i]]);
	subbrep_verts.insert(edge->Vertex(0)->m_vertex_index);
	subbrep_verts.insert(edge->Vertex(1)->m_vertex_index);
    }
    array_to_set(&faces, data->faces, data->faces_cnt);

    int vert_cnt = subbrep_verts.size();
    int ret = 0;
    switch (vert_cnt) {
	case 0:
	    bu_log("no verts???\n");
	    return 0;
	    break;
	case 1:
	    bu_log("one vertex - not a candidate for a planar volume\n");
	    return 0;
	    break;
	case 2:
	    bu_log("two vertices - not a candidate for a planar volume\n");
	    return 0;
	    break;
	case 3:
	    bu_log("three vertices - not a candidate for a planar volume\n");
	    return 0;
	    break;
	case 4:
	    if (point_set_is_arb4(data, &subbrep_verts) != 2) {
		return 0;
	    }
	    return 1;
	    break;
	case 5:
	    ret = point_set_is_arb5(data, &subbrep_verts);
	    return planar_switch(ret, data, &faces, &subbrep_verts);
	    break;
	case 6:
	    ret = point_set_is_arb6(data, &subbrep_verts);
	    return planar_switch(ret, data, &faces, &subbrep_verts);
	    break;
	case 7:
	    ret = point_set_is_arb7(data, &subbrep_verts);
	    return planar_switch(ret, data, &faces, &subbrep_verts);
	    break;
	case 8:
	    ret = point_set_is_arb8(data, &subbrep_verts);
	    return planar_switch(ret, data, &faces, &subbrep_verts);
	    break;
	default:
	    ret = point_set_is_arbn(data, &faces, &subbrep_verts, 1);
	    return planar_switch(ret, data, &faces, &subbrep_verts);
	    break;

    }
    return 0;
}


// TODO - need to check for self-intersecting planar objects - any
// such object needs to be further deconstructed, since it has
// components that may be making both positive and negative contributions


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
