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
#include "wdb.h"
#include "ged.h"
#include "../libbrep/shape_recognition.h"

#define ptout(p)  p.x << " " << p.y << " " << p.z

HIDDEN void
subbrep_obj_name(struct subbrep_object_data *data, struct bu_vls *name_root, struct bu_vls *name)
{
    if (!data || !name) return;
    switch (data->type) {
	case ARB6:
	    bu_vls_sprintf(name, "%s-arb6_%s.s", bu_vls_addr(name_root), bu_vls_addr(data->key));
	    return;
	case ARB8:
	    bu_vls_sprintf(name, "%s-arb8_%s.s", bu_vls_addr(name_root), bu_vls_addr(data->key));
	    return;
	case PLANAR_VOLUME:
	    bu_vls_sprintf(name, "%s-bot_%s.s", bu_vls_addr(name_root), bu_vls_addr(data->key));
	    return;
	case CYLINDER:
	    bu_vls_sprintf(name, "%s-rcc_%s.s", bu_vls_addr(name_root), bu_vls_addr(data->key));
	    return;
	case CONE:
	    bu_vls_sprintf(name, "%s-trc_%s.s", bu_vls_addr(name_root), bu_vls_addr(data->key));
	    return;
	case SPHERE:
	    bu_vls_sprintf(name, "%s-sph_%s.s", bu_vls_addr(name_root), bu_vls_addr(data->key));
	    return;
	case ELLIPSOID:
	    bu_vls_sprintf(name, "%s-ell_%s.s", bu_vls_addr(name_root), bu_vls_addr(data->key));
	    return;
	case TORUS:
	    bu_vls_sprintf(name, "%s-tor_%s.s", bu_vls_addr(name_root), bu_vls_addr(data->key));
	    return;
	case COMB:
	    bu_vls_sprintf(name, "%s-comb_%s.c", bu_vls_addr(name_root), bu_vls_addr(data->key));
	    return;
	default:
	    bu_vls_sprintf(name, "%s-brep_%s.s", bu_vls_addr(name_root), bu_vls_addr(data->key));
	    break;
    }
}


HIDDEN void
brep_to_bot(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct bu_vls *name_root)
{
    /* Triangulate faces and write out as a bot */
    struct bu_vls prim_name = BU_VLS_INIT_ZERO;
    int all_faces_cnt = 0;
    int *final_faces = NULL;

    if (!data->local_brep) {
	bu_log("No valid local brep?? bot %s failed\n", bu_vls_addr(data->key));
	return;
    }

    // Accumulate faces in a std::vector, since we don't know how many we're going to get
    std::vector<int> all_faces;

    /* Set up an initial comprehensive vertex array that will be used in
     * the final BoT build - all face definitions will ultimately index
     * into this array */
    if (data->local_brep->m_V.Count() == 0) {
	bu_log("No verts in brep?? bot %s failed\n", bu_vls_addr(data->key));
	return;
    }
    point_t *all_verts = (point_t *)bu_calloc(data->local_brep->m_V.Count(), sizeof(point_t), "bot verts");
    for (int vi = 0; vi < data->local_brep->m_V.Count(); vi++) {
	VMOVE(all_verts[vi], data->local_brep->m_V[vi].Point());
    }

    // Iterate over all faces in the brep.  TODO - should probably protect this with some planar checks...
    for (int s_it = 0; s_it < data->local_brep->m_F.Count(); s_it++) {
	const ON_BrepFace *b_face = &(data->local_brep->m_F[s_it]);
	/* There should be only an outer loop by this point in the process */
	/* TODO - should probably check that as an error-out condition... */
	const ON_BrepLoop *b_loop = b_face->OuterLoop();
	int *ffaces = NULL;
	int num_faces = subbrep_polygon_tri(data->local_brep, all_verts, b_loop->m_loop_index, &ffaces);
	for (int f_ind = 0; f_ind < num_faces*3; f_ind++) {
	    all_faces.push_back(ffaces[f_ind]);
	}
	if (ffaces) bu_free(ffaces, "free polygon face array");
	all_faces_cnt += num_faces;
    }

    /* Now we can build the final faces array for mk_bot */
    final_faces = (int *)bu_calloc(all_faces_cnt * 3, sizeof(int), "final bot verts");
    for (int i = 0; i < all_faces_cnt*3; i++) {
	final_faces[i] = all_faces[i];
    }

    /* Make the bot, using the data key for a unique name */
    subbrep_obj_name(data, name_root, &prim_name);
    if (mk_bot(wdbp, bu_vls_addr(&prim_name), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, data->brep->m_V.Count(), all_faces_cnt, (fastf_t *)all_verts, final_faces, (fastf_t *)NULL, (struct bu_bitv *)NULL)) {
	std::cout << "mk_bot failed for overall bot\n";
    }
}

int
subbrep_to_csg_arb6(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct bu_vls *name_root, struct wmember *wcomb)
{
    struct csg_object_params *params = data->params;
    if (data->type == ARB6) {
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data, name_root, &prim_name);

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
subbrep_to_csg_arb8(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct bu_vls *name_root, struct wmember *wcomb)
{
    struct csg_object_params *params = data->params;
    if (data->type == ARB8) {
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data, name_root, &prim_name);

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
subbrep_to_csg_planar(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct bu_vls *name_root, struct wmember *wcomb)
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
	if (!data->local_brep) {
	    bu_log("error - no local brep built for %s\n", bu_vls_addr(data->key));
	    return 0;
	}
	(void)brep_to_bot(data, wdbp, name_root);
	struct csg_object_params *params = data->params;
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data, name_root, &prim_name);
	//std::cout << bu_vls_addr(&prim_name) << ": " << params->bool_op << "\n";
	if (wcomb) (void)mk_addmember(bu_vls_addr(&prim_name), &((*wcomb).l), NULL, db_str2op(&(params->bool_op)));
	bu_vls_free(&prim_name);
	return 1;
    } else {
	return 0;
    }
}

HIDDEN int
subbrep_to_csg_cylinder(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct bu_vls *name_root, struct wmember *wcomb)
{
    struct csg_object_params *params = data->params;
    if (data->type == CYLINDER) {
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data, name_root, &prim_name);

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
subbrep_to_csg_conic(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct bu_vls *name_root, struct wmember *wcomb)
{
    struct csg_object_params *params = data->params;
    if (data->type == CONE) {
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data, name_root, &prim_name);

	mk_cone(wdbp, bu_vls_addr(&prim_name), params->origin, params->hv, params->height, params->radius, params->r2);
	//std::cout << bu_vls_addr(&prim_name) << ": " << params->bool_op << "\n";
	if (wcomb) (void)mk_addmember(bu_vls_addr(&prim_name), &((*wcomb).l), NULL, db_str2op(&(params->bool_op)));
	bu_vls_free(&prim_name);
	return 1;
    }
    return 0;
}

HIDDEN int
subbrep_to_csg_sphere(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct bu_vls *name_root, struct wmember *wcomb)
{
    struct csg_object_params *params = data->params;
    if (data->type == SPHERE) {
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data, name_root, &prim_name);

	mk_sph(wdbp, bu_vls_addr(&prim_name), params->origin, params->radius);
	//std::cout << bu_vls_addr(&prim_name) << ": " << params->bool_op << "\n";
	if (wcomb) (void)mk_addmember(bu_vls_addr(&prim_name), &((*wcomb).l), NULL, db_str2op(&(params->bool_op)));
	bu_vls_free(&prim_name);
	return 1;
    }
    return 0;
}

HIDDEN void
process_params(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct bu_vls *name_root, struct wmember *wcomb)
{
    switch (data->type) {
	case ARB6:
	    subbrep_to_csg_arb6(data, wdbp, name_root, wcomb);
	    break;
	case ARB8:
	    subbrep_to_csg_arb8(data, wdbp, name_root, wcomb);
	    break;
	case PLANAR_VOLUME:
	    subbrep_to_csg_planar(data, wdbp, name_root, wcomb);
	    break;
	case CYLINDER:
	    subbrep_to_csg_cylinder(data, wdbp, name_root, wcomb);
	    break;
	case CONE:
	    subbrep_to_csg_conic(data, wdbp, name_root, wcomb);
	    break;
	case SPHERE:
	    subbrep_to_csg_sphere(data, wdbp, name_root, wcomb);
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
make_shapes(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct bu_vls *name_root, struct wmember *pcomb, int depth)
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
	    subbrep_obj_name(data, name_root, &brep_name);
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
	    subbrep_obj_name(data, name_root, &comb_name);
	    BU_LIST_INIT(&wcomb.l);
	    if (data->planar_obj) {
	    //bu_log("%smake planar obj %s\n", bu_vls_addr(&spacer), bu_vls_addr(data->key));
		process_params(data->planar_obj, wdbp, name_root, &wcomb);
	    }
	    //bu_log("make comb %s\n", bu_vls_addr(data->key));
	    for (unsigned int i = 0; i < BU_PTBL_LEN(data->children); i++){
		struct subbrep_object_data *cdata = (struct subbrep_object_data *)BU_PTBL_GET(data->children,i);
		//std::cout << bu_vls_addr(&spacer) << "Making child shape " << bu_vls_addr(cdata->key) << " (" << cdata->type << "):\n";
		make_shapes(cdata, wdbp, name_root, &wcomb, depth+1);
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
	    process_params(data, wdbp, name_root, pcomb);
	}
    }
    return 0;
}

int
brep_to_csg(struct ged *gedp, struct directory *dp)
{
    struct rt_db_internal intern;
    struct rt_brep_internal *brep_ip = NULL;

    struct rt_wdb *wdbp = gedp->ged_wdbp;

    RT_DB_INTERNAL_INIT(&intern)

    if (rt_db_get_internal(&intern, dp, wdbp->dbip, NULL, &rt_uniresource) < 0) {
	return 1;
    }

    if (intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a B-Rep - aborting\n", dp->d_namep);
	return 1;
    } else {
	brep_ip = (struct rt_brep_internal *)intern.idb_ptr;
    }
    RT_BREP_CK_MAGIC(brep_ip);

    if (!rt_brep_valid(&intern, NULL)) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a valid B-Rep - aborting\n", dp->d_namep);
	return 1;
    }

    ON_Brep *brep = brep_ip->brep;
    struct wmember pcomb;
    struct bu_vls comb_name = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&comb_name, "csg_%s", dp->d_namep);
    BU_LIST_INIT(&pcomb.l);

    struct bu_ptbl *subbreps = find_subbreps(brep);
    struct bu_ptbl *subbreps_tree = find_top_level_hierarchy(subbreps);
    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++){
	struct subbrep_object_data *obj = (struct subbrep_object_data *)BU_PTBL_GET(subbreps, i);
	(void)make_shapes(obj, wdbp, &comb_name, NULL, 0);
    }

    struct wmember *ccomb = NULL;
    struct wmember *scomb = NULL;
    struct bu_vls obj_comb_name = BU_VLS_INIT_ZERO;
    struct bu_vls sub_comb_name = BU_VLS_INIT_ZERO;
    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps_tree); i++){
	struct subbrep_object_data *obj = (struct subbrep_object_data *)BU_PTBL_GET(subbreps_tree, i);
	struct bu_vls obj_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(obj, &comb_name, &obj_name);

	if (obj->params->bool_op == 'u') {
	    //print_subbrep_object(obj, "");
	    if (ccomb) {
		mk_lcomb(wdbp, bu_vls_addr(&obj_comb_name), ccomb, 0, NULL, NULL, NULL, 0);
		BU_PUT(ccomb, struct wmember);
		if (scomb) {
		    mk_lcomb(wdbp, bu_vls_addr(&sub_comb_name), scomb, 0, NULL, NULL, NULL, 0);
		    BU_PUT(scomb, struct wmember);
		    scomb = NULL;
		}
	    }
	    BU_GET(ccomb, struct wmember);
	    BU_LIST_INIT(&ccomb->l);
	    bu_vls_sprintf(&obj_comb_name, "%s-c_%s.c", bu_vls_addr(&comb_name), bu_vls_addr(obj->key));
	    bu_vls_sprintf(&sub_comb_name, "%s-s_%s.c", bu_vls_addr(&comb_name), bu_vls_addr(obj->key));
	    (void)mk_addmember(bu_vls_addr(&obj_comb_name), &(pcomb.l), NULL, db_str2op(&(obj->params->bool_op)));
	    (void)mk_addmember(bu_vls_addr(&obj_name), &((*ccomb).l), NULL, db_str2op(&(obj->params->bool_op)));
	} else {
	    char un = 'u';
	    if (!scomb) {
		BU_GET(scomb, struct wmember);
		BU_LIST_INIT(&scomb->l);
		(void)mk_addmember(bu_vls_addr(&sub_comb_name), &((*ccomb).l), NULL, db_str2op(&(obj->params->bool_op)));
	    }
	    //print_subbrep_object(obj, "  ");
	    (void)mk_addmember(bu_vls_addr(&obj_name), &((*scomb).l), NULL, db_str2op((const char *)&un));
	}
	//std::cout << bu_vls_addr(&obj_name) << ": " << obj->params->bool_op << "\n";
	//(void)mk_addmember(bu_vls_addr(&obj_name), &(pcomb.l), NULL, db_str2op(&(obj->params->bool_op)));
	bu_vls_free(&obj_name);
    }
    /* Make the last comb */
    if (ccomb) {
	mk_lcomb(wdbp, bu_vls_addr(&obj_comb_name), ccomb, 0, NULL, NULL, NULL, 0);
	BU_PUT(ccomb, struct wmember);
	if (scomb) {
	    mk_lcomb(wdbp, bu_vls_addr(&sub_comb_name), scomb, 0, NULL, NULL, NULL, 0);
	    BU_PUT(scomb, struct wmember);
	}
    }
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

    return 0;
}

int comb_to_csg(struct ged *gedp, struct directory *dp);

int
brep_csg_conversion_tree(struct ged *gedp, const union tree *oldtree, union tree *newtree)
{
    int ret = 0;
    *newtree = *oldtree;
    struct rt_comb_internal *comb = NULL;
    switch (oldtree->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    /* convert right */
	    bu_log("convert right\n");
	    comb = new rt_comb_internal;
	    newtree->tr_b.tb_right = new tree;
	    RT_TREE_INIT(newtree->tr_b.tb_right);
	    ret = brep_csg_conversion_tree(gedp, oldtree->tr_b.tb_right, newtree->tr_b.tb_right);
#if 0
	    if (ret) {
		delete newtree->tr_b.tb_right;
		break;
	    }
#endif
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    /* convert left */
	    bu_log("convert left\n");
	    BU_ALLOC(newtree->tr_b.tb_left, union tree);
	    RT_TREE_INIT(newtree->tr_b.tb_left);
	    ret = brep_csg_conversion_tree(gedp, oldtree->tr_b.tb_left, newtree->tr_b.tb_left);
#if 0
	    if (!ret) {
		comb->tree = newtree;
	    } else {
		delete newtree->tr_b.tb_left;
		delete newtree->tr_b.tb_right;
	    }
#endif
	    break;
	case OP_DB_LEAF:
	    struct bu_vls tmpname = BU_VLS_INIT_ZERO;
	    char *oldname = oldtree->tr_l.tl_name;
	    bu_vls_sprintf(&tmpname, "csg_%s", oldname);
	    newtree->tr_l.tl_name = (char*)bu_malloc(strlen(bu_vls_addr(&tmpname))+1, "char");
	    bu_log("have leaf: %s\n", oldtree->tr_l.tl_name);
	    bu_log("checking for: %s\n", bu_vls_addr(&tmpname));
	    if (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&tmpname), LOOKUP_QUIET) == RT_DIR_NULL) {
		struct directory *dir = db_lookup(gedp->ged_wdbp->dbip, oldname, LOOKUP_QUIET);

		if (dir != RT_DIR_NULL) {
		    if (dir->d_flags & RT_DIR_COMB) {
			ret = comb_to_csg(gedp, dir);
			if (ret) {
			    bu_vls_free(&tmpname);
			    break;
			}
			bu_strlcpy(newtree->tr_l.tl_name, bu_vls_addr(&tmpname), strlen(bu_vls_addr(&tmpname))+1);
			bu_vls_free(&tmpname);
			break;
		    }
		    // It's a primitive. If it's a b-rep object, convert it. Otherwise,
		    // just duplicate it. Might need better error codes from brep_to_csg for this...
		    if (brep_to_csg(gedp, dir)) {
			bu_log("non brep solid %s.\n", bu_vls_addr(&tmpname));
		    } else {
			bu_log("processed brep %s.\n", bu_vls_addr(&tmpname));
		    }
		} else {
		    bu_log("Cannot find %s.\n", oldname);
		    newtree = NULL;
		    ret = -1;
		}
	    } else {
		bu_log("%s already exists.\n", bu_vls_addr(&tmpname));
		bu_strlcpy(newtree->tr_l.tl_name, bu_vls_addr(&tmpname), strlen(bu_vls_addr(&tmpname))+1);
	    }
	    bu_vls_free(&tmpname);
	    break;
    }
    return 1;
}

int
comb_to_csg(struct ged *gedp, struct directory *dp)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb_internal = NULL;
    struct rt_wdb *wdbp = gedp->ged_wdbp;
    struct bu_vls comb_name = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&comb_name, "csg_%s", dp->d_namep);

    RT_DB_INTERNAL_INIT(&intern)

    if (rt_db_get_internal(&intern, dp, wdbp->dbip, NULL, &rt_uniresource) < 0) {
	return 1;
    }

    RT_CK_COMB(intern.idb_ptr);
    comb_internal = (struct rt_comb_internal *)intern.idb_ptr;

    if (comb_internal->tree == NULL) {
	// Empty tree
	int ret = wdb_export(wdbp, bu_vls_addr(&comb_name), comb_internal, ID_COMBINATION, 1);
	return ret;
    }

    RT_CK_TREE(comb_internal->tree);
    union tree *oldtree = comb_internal->tree;
    struct rt_comb_internal *new_internal;

    BU_ALLOC(new_internal, struct rt_comb_internal);
    *new_internal = *comb_internal;
    BU_ALLOC(new_internal->tree, union tree);
    RT_TREE_INIT(new_internal->tree);

    union tree *newtree = new_internal->tree;

    int ret = brep_csg_conversion_tree(gedp, oldtree, newtree);
    if (!ret) {
	ret = wdb_export(wdbp, bu_vls_addr(&comb_name), (void *)new_internal, ID_COMBINATION, 1);
    } else {
	bu_free(new_internal->tree, "tree");
	bu_free(new_internal, "rt_comb_internal");
    }

    return ret;
}

extern "C" int
_ged_brep_to_csg(struct ged *gedp, const char *dp_name)
{
    struct rt_wdb *wdbp = gedp->ged_wdbp;
    struct directory *dp = db_lookup(wdbp->dbip, dp_name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) return GED_ERROR;

    if (dp->d_flags & RT_DIR_COMB) {
	return comb_to_csg(gedp, dp) ? GED_ERROR : GED_OK;
    } else {
	return brep_to_csg(gedp, dp) ? GED_ERROR : GED_OK;
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
