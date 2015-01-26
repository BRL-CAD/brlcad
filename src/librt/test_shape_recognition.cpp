#include "common.h"

#include <map>
#include <set>
#include <queue>
#include <list>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"
#include "plot3.h"
#include "opennurbs.h"
#include "brep.h"
#include "../libbrep/shape_recognition.h"


struct model *
brep_to_nmg(struct subbrep_object_data *data, struct rt_wdb *wdbp)
{
    struct bu_vls prim_name = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&prim_name, "nmg_%s", bu_vls_addr(data->key));
    std::set<int> b_verts;
    std::vector<int> b_verts_array;
    std::map<int, int> b_verts_to_verts;
    struct model *m = nmg_mm();
    struct nmgregion *r = nmg_mrsv(m);
    struct shell *s = BU_LIST_FIRST(shell, &(r)->s_hd);
    struct faceuse **fu;         /* array of faceuses */
    struct vertex **verts;       /* Array of pointers to vertex structs */
    struct vertex ***loop_verts; /* Array of pointers to vertex structs to pass to nmg_cmface */

    struct bn_tol nmg_tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1.0 - 1e-6 };

    int point_cnt = 0;
    int face_cnt = 0;
    int max_edge_cnt = 0;

    // One loop to a face, and the object data has the set of loops that make
    // up this object.
    for (int s_it = 0; s_it < data->loops_cnt; s_it++) {
	ON_BrepLoop *b_loop = &(data->brep->m_L[data->loops[s_it]]);
	ON_BrepFace *b_face = b_loop->Face();
	face_cnt++;
	if (b_loop->m_ti.Count() > max_edge_cnt) max_edge_cnt = b_loop->m_ti.Count();
	for (int ti = 0; ti < b_loop->m_ti.Count(); ti++) {
	    ON_BrepTrim& trim = b_face->Brep()->m_T[b_loop->m_ti[ti]];
	    ON_BrepEdge& edge = b_face->Brep()->m_E[trim.m_ei];
	    if (b_verts.find(edge.Vertex(0)->m_vertex_index) == b_verts.end()) {
		b_verts.insert(edge.Vertex(0)->m_vertex_index);
		b_verts_array.push_back(edge.Vertex(0)->m_vertex_index);
		b_verts_to_verts[edge.Vertex(0)->m_vertex_index] = b_verts_array.size()-1;
	    }
	    if (b_verts.find(edge.Vertex(1)->m_vertex_index) == b_verts.end()) {
		b_verts.insert(edge.Vertex(1)->m_vertex_index);
		b_verts_array.push_back(edge.Vertex(1)->m_vertex_index);
		b_verts_to_verts[edge.Vertex(1)->m_vertex_index] = b_verts_array.size()-1;
	    }
	}
    }

    point_cnt = b_verts.size();

    verts = (struct vertex **)bu_calloc(point_cnt, sizeof(struct vertex *), "brep_to_nmg: verts");
    loop_verts = (struct vertex ***) bu_calloc(max_edge_cnt, sizeof(struct vertex **), "brep_to_nmg: loop_verts");
    fu = (struct faceuse **) bu_calloc(face_cnt, sizeof(struct faceuse *), "brep_to_nmg: fu");

    // Make the faces
    int face_count = 0;
    for (int s_it = 0; s_it < data->loops_cnt; s_it++) {
	int loop_length = 0;
	ON_BrepLoop *b_loop = &(data->brep->m_L[data->loops[s_it]]);
	ON_BrepFace *b_face = b_loop->Face();
	for (int ti = 0; ti < b_loop->m_ti.Count(); ti++) {
	    ON_BrepTrim& trim = b_face->Brep()->m_T[b_loop->m_ti[ti]];
	    ON_BrepEdge& edge = b_face->Brep()->m_E[trim.m_ei];
	    if (trim.m_bRev3d) {
		loop_verts[loop_length] = &(verts[b_verts_to_verts[edge.Vertex(0)->m_vertex_index]]);
	    } else {
		loop_verts[loop_length] = &(verts[b_verts_to_verts[edge.Vertex(1)->m_vertex_index]]);
	    }
	    loop_length++;
	}
	fu[face_count] = nmg_cmface(s, loop_verts, loop_length);
	face_count++;
    }

    for (int p = 0; p < point_cnt; p++) {
	ON_3dPoint pt = data->brep->m_V[b_verts_array[p]].Point();
	point_t nmg_pt;
	nmg_pt[0] = pt.x;
	nmg_pt[1] = pt.y;
	nmg_pt[2] = pt.z;
	nmg_vertex_gv(verts[p], pt);
    }

    for (int f = 0; f < face_cnt; f++) {
	nmg_fu_planeeqn(fu[f], &nmg_tol);
    }

    nmg_fix_normals(s, &nmg_tol);
    (void)nmg_mark_edges_real(&s->l.magic);
    /* Compute "geometry" for region and shell */
    nmg_region_a(r, &nmg_tol);

    /* Create the nmg primitive */
    mk_nmg(wdbp, bu_vls_addr(&prim_name), m);

    bu_vls_free(&prim_name);

    return m;
}


int
subbrep_to_csg_planar(struct subbrep_object_data *data, struct rt_wdb *wdbp)
{
    if (data->type == PLANAR_VOLUME) {
	// BRL-CAD's arbn primitive does not support concave shapes, and we want to use
	// simpler primitives than the generic nmg if the opportunity arises.  A heuristic
	// along the following lines may help:
	//
	// Step 1.  Check the number of vertices.  If the number is small enough that the
	//          volume might conceivably be expressed by an arb4-arb8, try that first.
	//
	// Step 2.  If the arb4-arb8 test fails, do the convex hull and see if all points
	//          are used by the hull.  If so, the shape is convex and may be expressed
	//          as an arbn.
	//
	// Step 3.  If the arbn test fails, construct sets of contiguous concave faces using
	//          the set of edges with one or more vertices not in the convex hull.  If
	//          those shapes are all convex, construct an arbn tree (or use simpler arb
	//          shapes if the subtractions may be so expressed).
	//
	// Step 4.  If the subtraction volumes in Step 3 are still not convex, cut our losses
	//          and proceed to the nmg primitive.  It may conceivably be worth some
	//          additional searches to spot convex subsets of shapes that can be more
	//          simply represented, but that is not particularly simple to do well
	//          and should wait until it is clear we would get a benefit from it.  Look
	//          at the arbn tessellation routine for a guide on how to set up the
	//          nmg - that's the most general of the arb* primitives and should be
	//          relatively close to what is needed here.
	(void)brep_to_nmg(data, wdbp);
	return 1;
    } else {
	return 0;
    }
}

int
subbrep_to_csg_cylinder(struct subbrep_object_data *data, struct rt_wdb *wdbp)
{
    if (data->type == CYLINDER) {
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&prim_name, "rcc_%s", bu_vls_addr(data->key));

	mk_rcc(wdbp, bu_vls_addr(&prim_name), data->params->origin, data->params->hv, data->params->radius);
	bu_vls_free(&prim_name);
	return 1;
    }
    return 0;
}


#if 0
int
is_cone(const object_data *data)
{
    int ret = 1;
    std::set<int>::iterator f_it;
    std::set<int> planar_surfaces;
    std::set<int> conic_surfaces;
    std::set<int> active_edges;
    // First, check surfaces.  If a surface is anything other than a plane or cone,
    // the verdict is no.  If we don't have one planar surface and one or more
    // conic surfaces, the verdict is no.
    for (f_it = data->faces.begin(); f_it != data->faces.end(); f_it++) {
	ON_BrepFace *used_face = &(data->brep->m_F[(*f_it)]);
	ON_Surface *temp_surface = (ON_Surface *)used_face->SurfaceOf();
	int surface_type = (int)GetSurfaceType(temp_surface);
	switch (surface_type) {
	    case SURFACE_PLANE:
		planar_surfaces.insert(*f_it);
		break;
	    case SURFACE_CONE:
		conic_surfaces.insert(*f_it);
		break;
	    default:
		return 0;
		break;
	}
    }
    if (planar_surfaces.size() != 1) return 0;
    if (conic_surfaces.size() < 1) return 0;

    // Second, check if all conic surfaces share the same base point, apex point and radius.
    ON_Cone cone;
    data->brep->m_F[*conic_surfaces.begin()].SurfaceOf()->IsCone(&cone);
    for (f_it = conic_surfaces.begin(); f_it != conic_surfaces.end(); f_it++) {
	ON_Cone f_cone;
	data->brep->m_F[(*f_it)].SurfaceOf()->IsCone(&f_cone);
	ON_3dPoint fbp = f_cone.BasePoint();
	ON_3dPoint bp = cone.BasePoint();
	if (fbp.DistanceTo(bp) > 0.01) return 0;
	ON_3dPoint fap = f_cone.ApexPoint();
	ON_3dPoint ap = cone.ApexPoint();
	if (fap.DistanceTo(ap) > 0.01) return 0;
	if (!(NEAR_ZERO(cone.radius - f_cone.radius, 0.01))) return 0;
    }

    // Third, see if the planar face and the planes of any non-linear edges from the conic
    // faces are coplanar. TODO - do we need to do this?
    ON_Plane p1;
    data->brep->m_F[*planar_surfaces.begin()].SurfaceOf()->IsPlanar(&p1);

    // Fourth, check that the conic axis and the planar face are perpendicular.  If they
    // are not it isn't fatal, but we need to add a subtracting tgc and return a comb
    // to handle this situation so for now for simplicity require the perpendicular condition
    if (p1.Normal().IsParallelTo(cone.Axis(), 0.01) == 0) {
	std::cout << "p1 Normal: " << p1.Normal().x << "," << p1.Normal().y << "," << p1.Normal().z << "\n";
	std::cout << "cone axis: " << cone.Axis().x << "," << cone.Axis().y << "," << cone.Axis().z << "\n";
	return 0;
    }

    // Fifth, remove degenerate edge sets. A degenerate edge set is defined as two
    // linear segments having the same two vertices.  (To be sure, we should probably
    // check curve directions in loops in some fashion...)
    std::set<int>::iterator e_it;
    std::set<int> degenerate;
    for (e_it = data->edges.begin(); e_it != data->edges.end(); e_it++) {
	if (degenerate.find(*e_it) == degenerate.end()) {
	    ON_BrepEdge& edge = data->brep->m_E[*e_it];
	    if (edge.EdgeCurveOf()->IsLinear()) {
		for (f_it = data->edges.begin(); f_it != data->edges.end(); f_it++) {
		    ON_BrepEdge& edge2 = data->brep->m_E[*f_it];
		    if (edge2.EdgeCurveOf()->IsLinear()) {
			if ((edge.Vertex(0)->Point() == edge2.Vertex(0)->Point() && edge.Vertex(1)->Point() == edge2.Vertex(1)->Point()) ||
				(edge.Vertex(1)->Point() == edge2.Vertex(0)->Point() && edge.Vertex(0)->Point() == edge2.Vertex(1)->Point()))
			{
			    degenerate.insert(*e_it);
			    degenerate.insert(*f_it);
			    break;
			}
		    }
		}
	    }
	}
    }
    active_edges = data->edges;
    for (e_it = degenerate.begin(); e_it != degenerate.end(); e_it++) {
	//std::cout << "erasing " << *e_it << "\n";
	active_edges.erase(*e_it);
    }
    //std::cout << "Active Edge set: ";
#if 0
    for (e_it = active_edges.begin(); e_it != active_edges.end(); e_it++) {
	std::cout << (int)(*e_it);
	f_it = e_it;
	f_it++;
	if (f_it != active_edges.end()) std::cout << ",";
    }
    std::cout << "\n";
#endif

    // Sixth, check for any remaining linear segments.  If we have a real
    // cone and not just a partial, all the linear segments should have
    // washed out.
    for (e_it = active_edges.begin(); e_it != active_edges.end(); e_it++) {
	ON_BrepEdge& edge = data->brep->m_E[*e_it];
	if (edge.EdgeCurveOf()->IsLinear()) return 0;
    }

    // Seventh, make sure all the curved edges are on the same circle.
    ON_Circle circle;
    int circle_set= 0;
    for (e_it = active_edges.begin(); e_it != active_edges.end(); e_it++) {
	ON_BrepEdge& edge = data->brep->m_E[*e_it];
	ON_Arc arc;
	if (edge.EdgeCurveOf()->IsArc(NULL, &arc, 0.01)) {
	    ON_Circle circ(arc.StartPoint(), arc.MidPoint(), arc.EndPoint());
	    if (!circle_set) {
		circle_set = 1;
		circle = circ;
	    }
	    if (!NEAR_ZERO(circ.Center().DistanceTo(circle.Center()), 0.01)){
		std::cout << "found extra circle - no go\n";
		return 0;
	    }
	}
    }

    point_t base; // base of cone
    vect_t hv; // height vector of cone
    fastf_t height;
    ON_3dVector hvect(cone.ApexPoint() - cone.BasePoint());

    base[0] = cone.BasePoint().x;
    base[1] = cone.BasePoint().y;
    base[2] = cone.BasePoint().z;
    hv[0] = hvect.x;
    hv[1] = hvect.y;
    hv[2] = hvect.z;
    height = MAGNITUDE(hv);
    VUNITIZE(hv);
    mk_cone(data->wdbp, data->key.c_str(), base, hv, height, cone.radius, 0.000001);

    return ret;
}
#endif


int
make_shape(struct subbrep_object_data *data, struct rt_wdb *wdbp)
{
    struct bu_vls brep_name = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&brep_name, "brep_%s", bu_vls_addr(data->key));
    switch (data->type) {
	case COMB:
	    bu_log("TODO - make comb here\n");
	    return 0;
	    break;
	case PLANAR_VOLUME:
	    return subbrep_to_csg_planar(data, wdbp);
	    break;
	case CYLINDER:
	    return subbrep_to_csg_cylinder(data, wdbp);
	    break;
	case CONE:
	    if (data->local_brep) {
		mk_brep(wdbp, bu_vls_addr(&brep_name), data->local_brep);
	    } else {
		bu_log("Warning - mk_brep called but data->local_brep is empty\n");
	    }
	    bu_vls_free(&brep_name);
	    return 0;
	    break;
	case SPHERE:
	    bu_vls_free(&brep_name);
	    return 0;
	    break;
	case ELLIPSOID:
	    bu_vls_free(&brep_name);
	    return 0;
	    break;
	case TORUS:
	    bu_vls_free(&brep_name);
	    return 0;
	    break;
	default:
	    if (data->local_brep) {
		mk_brep(wdbp, bu_vls_addr(&brep_name), data->local_brep);
	    } else {
		bu_log("Warning - mk_brep called but data->local_brep is empty\n");
	    }
	    bu_vls_free(&brep_name);

	    return 0; /* BREP */
	    break;
    }

    /* Shouldn't get here */
    return 0;
}
#if 0
/* generate new face(s) needed to close new obj, add to new obj and inverse of face to old obj */
void
close_objs(object_data *new_obj, object_data *old_obj)
{
   /* Step 1 - are we dealing with a subtraction?  If so, we have a bit more leeway */
   /* Step 2 - is the shape "well behaved"?  (irregular cylinder test is an example)  If it is, we
    * don't need to worry about using leeway from subtractions - otherwise, we do */
   /* Step 3 - if well behaved, generate new face(s) needed to close up this face and inverses of those faces for the old object. */
   /* Step 4 - if well behaved, add new faces to brep and update all necessary brep structures */
   /* Step 5 - if NOT well behaved and is a subtraction, determine necessary shape to encompass
    * subtraction volume.  Identify faces and curves no longer needed if subtraction is performed,
    * and simplify brep */
   /* Step 6 - update object data structures to reflect the new objects */
}


/* When we have a composite object, we must divide that object into
 * simpler objects that we can represent.  The "seed" face is the
 * index of the face with the most complicated surface type in the
 * input object_data set.  split_object's job is to identify the
 * set of one or more faces, starting with seed_face, that will define
 * the new candidate object_data set and build it up.  "Missing"
 * faces not in the model but needed to define a closed volume will
 * be added to the ON_Brep and to the object_data set.  When a new face
 * is added, all existing 3D edges must be re-used - no duplicates
 * are allowed. The input object will be altered to no longer contain
 * data now exclusive to the new_obj set */
bool
split_object(comb_data *curr_comb, object_data *input)
{
    object_data tmp_obj = *input;
    //int is_obj = find_shape(&tmp_obj);
    while (!is_obj) {
	int seed_face = highest_order_face(&tmp_obj);
	/* TODO - for cylinders, check for a mating face with the same axis - want to go for the rcc if we can */
	object_data *new_obj = new object_data(input->brep, input->wdbp);
	/* TODO - add seed face to new obj and remove from tmp_obj */
	new_obj->faces.insert(seed_face);
	tmp_obj.faces.erase(seed_face);
	/* generate new face(s) needed to close new obj, add to new obj and inverse of face to old obj */
	close_objs(new_obj, &tmp_obj);
	/* If new faces cannot be generated, return false */
	curr_comb->objects.push_back(&(*new_obj));
	//is_obj = find_shape(&tmp_obj);
    }
    object_data *final_obj = new object_data(input->brep, input->wdbp);
    final_obj->key = tmp_obj.key;
    final_obj->faces = tmp_obj.faces;
    final_obj->loops = tmp_obj.loops;
    final_obj->edges = tmp_obj.edges;
    final_obj->negative_object = tmp_obj.negative_object;
    final_obj->fol = tmp_obj.fol;
    final_obj->fil = tmp_obj.fil;
    final_obj->brep = tmp_obj.brep;
    final_obj->wdbp = tmp_obj.wdbp;
    curr_comb->objects.push_back(final_obj);
    return true;
}

#endif

int
main(int argc, char *argv[])
{
    struct db_i *dbip;
    struct rt_wdb *wdbp;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_brep_internal *brep_ip = NULL;

    if (argc != 3) {
	bu_exit(1, "Usage: %s file.g object", argv[0]);
    }

    dbip = db_open(argv[1], DB_OPEN_READWRITE);
    if (dbip == DBI_NULL) {
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
    }

    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);

    if (db_dirbuild(dbip) < 0)
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);

    dp = db_lookup(dbip, argv[2], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_exit(1, "ERROR: Unable to look up object %s\n", argv[2]);
    }

    RT_DB_INTERNAL_INIT(&intern)

    if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0) {
        bu_exit(1, "ERROR: Unable to get internal representation of %s\n", argv[2]);
    }

    if (intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_exit(1, "ERROR: object %s does not appear to be of type BRep\n", argv[2]);
    } else {
	brep_ip = (struct rt_brep_internal *)intern.idb_ptr;
    }
    RT_BREP_CK_MAGIC(brep_ip);

    ON_Brep *brep = brep_ip->brep;

    struct bu_ptbl *subbreps = find_subbreps(brep);
    //split_subbreps(subbreps);
    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++){
	struct subbrep_object_data *obj = (struct subbrep_object_data *)BU_PTBL_GET(subbreps, i);
	//print_subbrep_object(obj, "");
	if (obj->type == BREP) {
	    (void)make_shape(obj, wdbp);
	    //subbrep_split(obj);
	    for (unsigned int j = 0; j < BU_PTBL_LEN(obj->children); j++){
		struct subbrep_object_data *cobj = (struct subbrep_object_data *)BU_PTBL_GET(obj->children, j);
		//print_subbrep_object(cobj, "  ");
		//subbrep_csg_assemble(cobj);
	    }
	} else {
	    (void)make_shape(obj, wdbp);
	}
	subbrep_object_free(obj);
	BU_PUT(obj, struct subbrep_object_data);
    }
    bu_ptbl_free(subbreps);
    BU_PUT(subbreps, struct bu_ptbl);

    db_close(dbip);

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
