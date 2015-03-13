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

#define ptout(p)  p.x << " " << p.y << " " << p.z

HIDDEN void
subbrep_obj_name(struct subbrep_object_data *data, struct bu_vls *name)
{
    if (!data || !name) return;
    switch (data->type) {
	case ARB6:
	    bu_vls_sprintf(name, "arb6_%s.s", bu_vls_addr(data->key));
	    return;
	case ARB8:
	    bu_vls_sprintf(name, "arb8_%s.s", bu_vls_addr(data->key));
	    return;
	case PLANAR_VOLUME:
	    bu_vls_sprintf(name, "bot_%s.s", bu_vls_addr(data->key));
	    return;
	case CYLINDER:
	    bu_vls_sprintf(name, "rcc_%s.s", bu_vls_addr(data->key));
	    return;
	case CONE:
	    bu_vls_sprintf(name, "trc_%s.s", bu_vls_addr(data->key));
	    return;
	case SPHERE:
	    bu_vls_sprintf(name, "sph_%s.s", bu_vls_addr(data->key));
	    return;
	case ELLIPSOID:
	    bu_vls_sprintf(name, "ell_%s.s", bu_vls_addr(data->key));
	    return;
	case TORUS:
	    bu_vls_sprintf(name, "tor_%s.s", bu_vls_addr(data->key));
	    return;
	case COMB:
	    bu_vls_sprintf(name, "comb_%s.c", bu_vls_addr(data->key));
	    return;
	default:
	    bu_vls_sprintf(name, "brep_%s.s", bu_vls_addr(data->key));
	    break;
    }
}


HIDDEN void
brep_to_bot(struct subbrep_object_data *data, struct rt_wdb *wdbp)
{
    /* Triangulate faces and write out as a bot */
    struct bu_vls prim_name = BU_VLS_INIT_ZERO;
    int all_faces_cnt = 0;
    int face_error = 0;
    int *final_faces = NULL;

    // Accumulate faces in a std::vector, since we don't know how many we're going to get
    std::vector<int> all_faces;

    /* Set up an initial comprehensive vertex array that will be used in
     * the final BoT build - all face definitions will ultimately index
     * into this array */
    point_t *all_verts = (point_t *)bu_calloc(data->brep->m_V.Count(), sizeof(point_t), "bot verts");
    for (int vi = 0; vi < data->brep->m_V.Count(); vi++) {
	VMOVE(all_verts[vi], data->brep->m_V[vi].Point());
    }

    // Iterate over all faces in the brep.  TODO - should probably protect this with some planar checks...
    for (int s_it = 0; s_it < data->brep->m_F.Count(); s_it++) {
	int num_faces;
	int *faces;
	const ON_BrepFace *b_face = &(data->brep->m_F[s_it]);

	/* There should be only an outer loop by this point in the process */
	/* TODO - should probably check that as an error-out condition... */
	const ON_BrepLoop *b_loop = b_face->OuterLoop();

	/* We need to build a 2D vertex list for the triangulation, and as long
	 * as we make sure the vertex mapping accounts for flipped trims in 3D,
	 * the point indices returned for the 2D triangulation will correspond
	 * to the correct 3D points without any additional work.  In essence,
	 * we use the fact that the BRep gives us a ready-made 2D point
	 * parameterization to same some work.*/
	point2d_t *verts2d = (point2d_t *)bu_calloc(b_loop->m_ti.Count(), sizeof(point2d_t), "bot verts");

	/* The triangulation will use only the points in the temporary verts2d
	 * array and return with faces indexed accordingly, so we need a map to
	 * translate them back to our final vert array */
	std::map<int, int> local_to_verts;

	/* Now the fun begins.  If we have an outer loop that isn't CCW, we
	 * need to assemble our verts2d polygon backwards */
	if (data->brep->LoopDirection(data->local_brep->m_L[b_loop->m_loop_index]) != 1) {
	    for (int ti = 0; ti < b_loop->m_ti.Count(); ti++) {
		int ti_rev = b_loop->m_ti.Count() - 1 - ti;
		const ON_BrepTrim *trim = &(b_face->Brep()->m_T[b_loop->m_ti[ti_rev]]);
		const ON_BrepEdge *edge = &(b_face->Brep()->m_E[trim->m_ei]);
		const ON_Curve *trim_curve = trim->TrimCurveOf();
		ON_2dPoint cp = trim_curve->PointAt(trim_curve->Domain().Min());
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

	/* The real work - triangulate the 2D polygon to find out triangles for
	 * this particular B-Rep face */
	face_error = bn_polygon_triangulate(&faces, &num_faces, (const point2d_t *)verts2d, b_loop->m_ti.Count());
	if (face_error || !faces) {
	    std::cout << "bot build failed for face " << b_face->m_face_index << "\n";
	    bu_free(verts2d, "free tmp 2d vertex array");
	    bu_free(all_verts, "free top level vertex array");
	    return;
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
	    all_faces_cnt += num_faces;
	    bu_free(verts2d, "free tmp 2d vertex array");
	}

	/* Uncomment this code to create per-face bots for debugging purposes */
	/*
	bu_vls_sprintf(&prim_name, "bot_%s_face_%d.s", bu_vls_addr(data->key), b_face->m_face_index);
	if (mk_bot(wdbp, bu_vls_addr(&prim_name), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, b_loop->m_ti.Count(), num_faces, (fastf_t *)verts2d, faces, (fastf_t *)NULL, (struct bu_bitv *)NULL)) {
	    std::cout << "mk_bot failed for face " << b_face->m_face_index << "\n";
	}
	*/
    }

    /* Now we can build the final faces array for mk_bot */
    final_faces = (int *)bu_calloc(all_faces_cnt * 3, sizeof(int), "final bot verts");
    for (int i = 0; i < all_faces_cnt*3; i++) {
	final_faces[i] = all_faces[i];
    }

    /* Make the bot, using the data key for a unique name */
    subbrep_obj_name(data, &prim_name);
    if (mk_bot(wdbp, bu_vls_addr(&prim_name), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, data->brep->m_V.Count(), all_faces_cnt, (fastf_t *)all_verts, final_faces, (fastf_t *)NULL, (struct bu_bitv *)NULL)) {
	std::cout << "mk_bot failed for overall bot\n";
    }
}

int
subbrep_to_csg_arb6(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct wmember *wcomb)
{
    struct csg_object_params *params = data->params;
    if (data->type == ARB6) {
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data, &prim_name);

	mk_arb6(wdbp, bu_vls_addr(&prim_name), (const fastf_t *)params->p);
	//std::cout << bu_vls_addr(&prim_name) << ": " << params->bool_op << "\n";
	if (wcomb) (void)mk_addmember(bu_vls_addr(&prim_name), &((*wcomb).l), NULL, db_str2op(&(params->bool_op)));
	bu_vls_free(&prim_name);
	return 1;
    } else {
	return 0;
    }
}

HIDDEN int
subbrep_to_csg_arb8(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct wmember *wcomb)
{
    struct csg_object_params *params = data->params;
    if (data->type == ARB8) {
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data, &prim_name);

	mk_arb8(wdbp, bu_vls_addr(&prim_name), (const fastf_t *)params->p);
	//std::cout << bu_vls_addr(&prim_name) << ": " << params->bool_op << "\n";
	if (wcomb) (void)mk_addmember(bu_vls_addr(&prim_name), &((*wcomb).l), NULL, db_str2op(&(params->bool_op)));
	bu_vls_free(&prim_name);
	return 1;
    } else {
	return 0;
    }
}

HIDDEN int
subbrep_to_csg_planar(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct wmember *wcomb)
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
	(void)brep_to_bot(data, wdbp);
	struct csg_object_params *params = data->params;
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data, &prim_name);
	//std::cout << bu_vls_addr(&prim_name) << ": " << params->bool_op << "\n";
	if (wcomb) (void)mk_addmember(bu_vls_addr(&prim_name), &((*wcomb).l), NULL, db_str2op(&(params->bool_op)));
	bu_vls_free(&prim_name);
	return 1;
    } else {
	return 0;
    }
}

HIDDEN int
subbrep_to_csg_cylinder(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct wmember *wcomb)
{
    struct csg_object_params *params = data->params;
    if (data->type == CYLINDER) {
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data, &prim_name);

	int ret = mk_rcc(wdbp, bu_vls_addr(&prim_name), params->origin, params->hv, params->radius);
	if (ret) {
	    std::cout << "problem making " << bu_vls_addr(&prim_name) << "\n";
	}
	//std::cout << bu_vls_addr(&prim_name) << ": " << params->bool_op << "\n";
	if (wcomb) (void)mk_addmember(bu_vls_addr(&prim_name), &((*wcomb).l), NULL, db_str2op(&(params->bool_op)));
	bu_vls_free(&prim_name);
	return 1;
    }
    return 0;
}

HIDDEN int
subbrep_to_csg_conic(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct wmember *wcomb)
{
    struct csg_object_params *params = data->params;
    if (data->type == CONE) {
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data, &prim_name);

	mk_cone(wdbp, bu_vls_addr(&prim_name), params->origin, params->hv, params->height, params->radius, params->r2);
	//std::cout << bu_vls_addr(&prim_name) << ": " << params->bool_op << "\n";
	if (wcomb) (void)mk_addmember(bu_vls_addr(&prim_name), &((*wcomb).l), NULL, db_str2op(&(params->bool_op)));
	bu_vls_free(&prim_name);
	return 1;
    }
    return 0;
}

HIDDEN int
subbrep_to_csg_sphere(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct wmember *wcomb)
{
    struct csg_object_params *params = data->params;
    if (data->type == SPHERE) {
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data, &prim_name);

	mk_sph(wdbp, bu_vls_addr(&prim_name), params->origin, params->radius);
	//std::cout << bu_vls_addr(&prim_name) << ": " << params->bool_op << "\n";
	if (wcomb) (void)mk_addmember(bu_vls_addr(&prim_name), &((*wcomb).l), NULL, db_str2op(&(params->bool_op)));
	bu_vls_free(&prim_name);
	return 1;
    }
    return 0;
}

HIDDEN void
process_params(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct wmember *wcomb)
{
    switch (data->type) {
	case ARB6:
	    subbrep_to_csg_arb6(data, wdbp, wcomb);
	    break;
	case ARB8:
	    subbrep_to_csg_arb8(data, wdbp, wcomb);
	    break;
	case PLANAR_VOLUME:
	    subbrep_to_csg_planar(data, wdbp, wcomb);
	    break;
	case CYLINDER:
	    subbrep_to_csg_cylinder(data, wdbp, wcomb);
	    break;
	case CONE:
	    subbrep_to_csg_conic(data, wdbp, wcomb);
	    break;
	case SPHERE:
	    subbrep_to_csg_sphere(data, wdbp, wcomb);
	    break;
	case ELLIPSOID:
	    break;
	case TORUS:
	    break;
	default:
	    break;
    }
}


HIDDEN int
make_shapes(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct wmember *pcomb, int depth)
{
    struct bu_vls spacer = BU_VLS_INIT_ZERO;
    for (int i = 0; i < depth; i++)
	bu_vls_printf(&spacer, " ");
    //std::cout << bu_vls_addr(&spacer) << "Making shape for " << bu_vls_addr(data->key) << "\n";
#if 0
    if (data->planar_obj && data->planar_obj->local_brep) {
	struct bu_vls brep_name = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&brep_name, "planar_%s.s", bu_vls_addr(data->key));
	mk_brep(wdbp, bu_vls_addr(&brep_name), data->planar_obj->local_brep);
	bu_vls_free(&brep_name);
    }
#endif
    if (data->type == BREP) {
	if (data->local_brep) {
	    struct bu_vls brep_name = BU_VLS_INIT_ZERO;
	    subbrep_obj_name(data, &brep_name);
	    mk_brep(wdbp, bu_vls_addr(&brep_name), data->local_brep);
	    // TODO - almost certainly need to do more work to get correct booleans
	    //std::cout << bu_vls_addr(&brep_name) << ": " << data->params->bool_op << "\n";
	    if (pcomb) (void)mk_addmember(bu_vls_addr(&brep_name), &(pcomb->l), NULL, db_str2op(&(data->params->bool_op)));
	    bu_vls_free(&brep_name);
	} else {
	    bu_log("Warning - mk_brep called but data->local_brep is empty\n");
	}
    } else {
	if (data->type == COMB) {
	    struct wmember wcomb;
	    struct bu_vls comb_name = BU_VLS_INIT_ZERO;
	    struct bu_vls member_name = BU_VLS_INIT_ZERO;
	    subbrep_obj_name(data, &comb_name);
	    BU_LIST_INIT(&wcomb.l);
	    if (data->planar_obj) {
	    //bu_log("%smake planar obj %s\n", bu_vls_addr(&spacer), bu_vls_addr(data->key));
		process_params(data->planar_obj, wdbp, &wcomb);
	    }
	    //bu_log("make comb %s\n", bu_vls_addr(data->key));
	    for (unsigned int i = 0; i < BU_PTBL_LEN(data->children); i++){
		struct subbrep_object_data *cdata = (struct subbrep_object_data *)BU_PTBL_GET(data->children,i);
		//std::cout << bu_vls_addr(&spacer) << "Making child shape " << bu_vls_addr(cdata->key) << " (" << cdata->type << "):\n";
		make_shapes(cdata, wdbp, &wcomb, depth+1);
		subbrep_object_free(cdata);
	    }
	    mk_lcomb(wdbp, bu_vls_addr(&comb_name), &wcomb, 0, NULL, NULL, NULL, 0);

	    // TODO - almost certainly need to do more work to get correct booleans
	    //std::cout << bu_vls_addr(&comb_name) << ": " << data->params->bool_op << "\n";
	    if (pcomb) (void)mk_addmember(bu_vls_addr(&comb_name), &(pcomb->l), NULL, db_str2op(&(data->params->bool_op)));

	    bu_vls_free(&member_name);
	    bu_vls_free(&comb_name);
	} else {
	    //std::cout << "type: " << data->type << "\n";
	    //bu_log("%smake solid %s\n", bu_vls_addr(&spacer), bu_vls_addr(data->key));
	    process_params(data, wdbp, pcomb);
	}
    }
    return 0;
}

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
    struct wmember pcomb;
    struct bu_vls comb_name = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&comb_name, "%s_csg.c", dp->d_namep);
    BU_LIST_INIT(&pcomb.l);

    struct bu_ptbl *subbreps = find_subbreps(brep);
    struct bu_ptbl *subbreps_tree = find_top_level_hierarchy(subbreps);
    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++){
	struct subbrep_object_data *obj = (struct subbrep_object_data *)BU_PTBL_GET(subbreps, i);
	//print_subbrep_object(obj, "");
	(void)make_shapes(obj, wdbp, NULL, 0);
	//subbrep_object_free(obj);
	//BU_PUT(obj, struct subbrep_object_data);
    }

    struct wmember *ccomb = NULL;
    struct bu_vls obj_comb_name = BU_VLS_INIT_ZERO;
    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps_tree); i++){
	struct subbrep_object_data *obj = (struct subbrep_object_data *)BU_PTBL_GET(subbreps_tree, i);
	struct bu_vls obj_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(obj, &obj_name);
	if (obj->params->bool_op == 'u') {
	    if (ccomb) {
		mk_lcomb(wdbp, bu_vls_addr(&obj_comb_name), ccomb, 0, NULL, NULL, NULL, 0);
		BU_PUT(ccomb, struct wmember);
	    }
	    BU_GET(ccomb, struct wmember);
	    BU_LIST_INIT(&ccomb->l);
	    bu_vls_sprintf(&obj_comb_name, "c_%s.c", bu_vls_addr(obj->key));
	    (void)mk_addmember(bu_vls_addr(&obj_comb_name), &(pcomb.l), NULL, db_str2op(&(obj->params->bool_op)));
	    (void)mk_addmember(bu_vls_addr(&obj_name), &((*ccomb).l), NULL, db_str2op(&(obj->params->bool_op)));
	} else {
	    (void)mk_addmember(bu_vls_addr(&obj_name), &((*ccomb).l), NULL, db_str2op(&(obj->params->bool_op)));
	}
	//std::cout << bu_vls_addr(&obj_name) << ": " << obj->params->bool_op << "\n";
	//(void)mk_addmember(bu_vls_addr(&obj_name), &(pcomb.l), NULL, db_str2op(&(obj->params->bool_op)));
	bu_vls_free(&obj_name);
    }
    /* Make the last comb */
    mk_lcomb(wdbp, bu_vls_addr(&obj_comb_name), ccomb, 0, NULL, NULL, NULL, 0);
    BU_PUT(ccomb, struct wmember);

    bu_vls_free(&obj_comb_name);

    // Free memory
    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++){
	struct subbrep_object_data *obj = (struct subbrep_object_data *)BU_PTBL_GET(subbreps, i);
	for (unsigned int j = 0; j < BU_PTBL_LEN(obj->children); j++){
	    struct subbrep_object_data *cdata = (struct subbrep_object_data *)BU_PTBL_GET(obj->children,j);
	    subbrep_object_free(cdata);
	}
	subbrep_object_free(obj);
	BU_PUT(obj, struct subbrep_object_data);
    }

    bu_ptbl_free(subbreps);
    bu_ptbl_free(subbreps_tree);
    BU_PUT(subbreps, struct bu_ptbl);
    BU_PUT(subbreps_tree, struct bu_ptbl);
    // TODO - probably should be region
    mk_lcomb(wdbp, bu_vls_addr(&comb_name), &pcomb, 0, NULL, NULL, NULL, 0);

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
