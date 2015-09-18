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
#include "analyze.h"
#include "ged.h"
#include "../libbrep/shape_recognition.h"

#define ptout(p)  p.x << " " << p.y << " " << p.z

HIDDEN void
assemble_vls_str(struct bu_vls *vls, int *edges, int cnt)
{
    bu_vls_sprintf(vls, "%d", edges[0]);
    for (int i = 1; i < cnt; i++) {
	bu_vls_printf(vls, ",%d", edges[i]);
    }
}

HIDDEN void
obj_add_attr_key(struct subbrep_island_data *data, struct rt_wdb *wdbp, const char *obj_name)
{
    struct bu_attribute_value_set avs;
    struct directory *dp;

    if (!data || !wdbp || !obj_name) return;

    dp = db_lookup(wdbp->dbip, obj_name, LOOKUP_QUIET);

    if (dp == RT_DIR_NULL) return;

    bu_avs_init_empty(&avs);

    if (db5_get_attributes(wdbp->dbip, &avs, dp)) return;

    (void)bu_avs_add(&avs, "bfaces", bu_vls_addr(data->key));
    if (data->edges_cnt > 0) {
	struct bu_vls val = BU_VLS_INIT_ZERO;
	assemble_vls_str(&val, data->edges, data->edges_cnt);
	(void)bu_avs_add(&avs, "bedges", bu_vls_addr(&val));
	bu_vls_free(&val);
    }

    if (db5_replace_attributes(dp, &avs, wdbp->dbip)) {
	bu_avs_free(&avs);
	return;
    }
}

HIDDEN void
subbrep_obj_name(int type, int id, const char *root, struct bu_vls *name)
{
    if (!root || !name) return;
    switch (type) {
	case ARB6:
	    bu_vls_printf(name, "%s-arb6_%d.s", root, id);
	    return;
	case ARB8:
	    bu_vls_printf(name, "%s-arb8_%d.s", root, id);
	    return;
	case ARBN:
	    bu_vls_printf(name, "%s-arbn_%d.s", root, id);
	    return;
	case PLANAR_VOLUME:
	    bu_vls_printf(name, "%s-bot_%d.s", root, id);
	    return;
	case CYLINDER:
	    bu_vls_printf(name, "%s-rcc_%d.s", root, id);
	    return;
	case CONE:
	    bu_vls_printf(name, "%s-trc_%d.s", root, id);
	    return;
	case SPHERE:
	    bu_vls_printf(name, "%s-sph_%d.s", root, id);
	    return;
	case ELLIPSOID:
	    bu_vls_printf(name, "%s-ell_%d.s", root, id);
	    return;
	case TORUS:
	    bu_vls_printf(name, "%s-tor_%d.s", root, id);
	    return;
	case COMB:
	    bu_vls_printf(name, "%s-comb_%d.c", root, id);
	    return;
	default:
	    bu_vls_printf(name, "%s-brep_%d.s", root, id);
	    break;
    }
}


HIDDEN int
subbrep_to_csg_arbn(struct bu_vls *msgs, struct csg_object_params *data, struct rt_wdb *wdbp, const char *pname)
{
    if (!msgs || !data || !wdbp || !pname) return 0;
    if (data->type == ARBN) {
	/* Make the arbn, using the data key for a unique name */
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data->type, data->id, pname, &prim_name);
	if (mk_arbn(wdbp, bu_vls_addr(&prim_name), data->plane_cnt, data->planes)) {
	    if (msgs) bu_vls_printf(msgs, "mk_arbn failed for %s\n", bu_vls_addr(&prim_name));
	} else {
	    //obj_add_attr_key(data, wdbp, bu_vls_addr(&prim_name));
	}
	bu_vls_free(&prim_name);
	return 1;
    } else {
	return 0;
    }
}

HIDDEN int
subbrep_to_csg_planar(struct bu_vls *msgs, struct csg_object_params *data, struct rt_wdb *wdbp, const char *pname)
{
    if (!msgs || !data || !wdbp || !pname) return 0;
    if (data->type == PLANAR_VOLUME) {
	/* Make the bot, using the data key for a unique name */
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data->type, data->id, pname, &prim_name);
	if (mk_bot(wdbp, bu_vls_addr(&prim_name), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, data->vert_cnt, data->face_cnt, (fastf_t *)data->verts, data->faces, (fastf_t *)NULL, (struct bu_bitv *)NULL)) {
	    if (msgs) bu_vls_printf(msgs, "mk_bot failed for overall bot\n");
	} else {
	    //obj_add_attr_key(data, wdbp, bu_vls_addr(&prim_name));
	}
	bu_vls_free(&prim_name);
	return 1;
    } else {
	return 0;
    }
}

HIDDEN int
subbrep_to_csg_cylinder(struct bu_vls *msgs, struct csg_object_params *data, struct rt_wdb *wdbp, const char *pname)
{
    if (!msgs || !data || !wdbp || !pname) return 0;
    if (data->type == CYLINDER) {
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data->type, data->id, pname, &prim_name);
	int ret = mk_rcc(wdbp, bu_vls_addr(&prim_name), data->origin, data->hv, data->radius);
	if (ret) {
	    //std::cout << "problem making " << bu_vls_addr(&prim_name) << "\n";
	}
#if 0
       	else {
	    obj_add_attr_key(params, wdbp, bu_vls_addr(&prim_name));
	}
#endif
	bu_vls_free(&prim_name);
	return 1;
    }
    return 0;
}


HIDDEN void
csg_obj_process(struct bu_vls *msgs, struct csg_object_params *data, struct rt_wdb *wdbp, const char *pname)
{
#if 0
    struct bu_vls prim_name = BU_VLS_INIT_ZERO;
    subbrep_obj_name(data->type, data->id, pname, &prim_name);
    struct directory *dp = db_lookup(wdbp->dbip, bu_vls_addr(&prim_name), LOOKUP_QUIET);
    // Don't recreate it
    if (dp != RT_DIR_NULL) {
	//bu_log("already made %s\n", bu_vls_addr(data->obj_name));
	return;
    }
#endif

    switch (data->type) {
	case ARB6:
	    //subbrep_to_csg_arb6(data, wdbp, pname, wcomb);
	    break;
	case ARB8:
	    //subbrep_to_csg_arb8(data, wdbp, pname, wcomb);
	    break;
	case ARBN:
	    subbrep_to_csg_arbn(msgs, data, wdbp, pname);
	    break;
	case PLANAR_VOLUME:
	    subbrep_to_csg_planar(msgs, data, wdbp, pname);
	    break;
	case CYLINDER:
	    subbrep_to_csg_cylinder(msgs, data, wdbp, pname);
	    break;
	case CONE:
	    //subbrep_to_csg_conic(data, wdbp, pname, wcomb);
	    break;
	case SPHERE:
	    //subbrep_to_csg_sphere(data, wdbp, id, wcomb);
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
make_shoal(struct bu_vls *msgs, struct subbrep_shoal_data *data, struct rt_wdb *wdbp, const char *rname)
{


    struct wmember wcomb;
    struct bu_vls prim_name = BU_VLS_INIT_ZERO;
    struct bu_vls comb_name = BU_VLS_INIT_ZERO;
    if (!data || !data->params) {
	if (msgs) bu_vls_printf(msgs, "Error! invalid shoal.\n");
	return 0;
    }

    struct bu_vls key = BU_VLS_INIT_ZERO;
    set_key(&key, data->loops_cnt, data->loops);
    bu_log("Processing shoal %s from island %s\n", bu_vls_addr(&key), bu_vls_addr(data->i->key));

    if (data->type == COMB) {
	BU_LIST_INIT(&wcomb.l);
	subbrep_obj_name(data->type, data->id, rname, &comb_name);
	bu_log("Created %s\n", bu_vls_addr(&comb_name));
	csg_obj_process(msgs, data->params, wdbp, rname);
	subbrep_obj_name(data->params->type, data->params->id, rname, &prim_name);
	bu_log("  %c %s\n", data->params->bool_op, bu_vls_addr(&prim_name));
	(void)mk_addmember(bu_vls_addr(&prim_name), &(wcomb.l), NULL, db_str2op(&(data->params->bool_op)));
	for (unsigned int i = 0; i < BU_PTBL_LEN(data->sub_params); i++) {
	    struct csg_object_params *c = (struct csg_object_params *)BU_PTBL_GET(data->sub_params, i);
	    csg_obj_process(msgs, c, wdbp, rname);
	    bu_vls_trunc(&prim_name, 0);
	    subbrep_obj_name(c->type, c->id, rname, &prim_name);
	    bu_log("     %c %s\n", c->bool_op, bu_vls_addr(&prim_name));
	    (void)mk_addmember(bu_vls_addr(&prim_name), &(wcomb.l), NULL, db_str2op(&(c->bool_op)));
	}
	mk_lcomb(wdbp, bu_vls_addr(&comb_name), &wcomb, 0, NULL, NULL, NULL, 0);
    } else {
	subbrep_obj_name(data->params->type, data->params->id, rname, &prim_name);
	csg_obj_process(msgs, data->params, wdbp, rname);
	bu_log("Created %s\n", bu_vls_addr(&prim_name));
    }
    return 1;
}

HIDDEN int
make_island(struct bu_vls *msgs, struct subbrep_island_data *data, struct rt_wdb *wdbp, const char *rname, struct wmember *pcomb)
{
    char *bool_op = &(data->nucleus->params->bool_op);
    int failed = 0;
    struct wmember wcomb;
    int type = -1;

    struct bu_vls shoal_name = BU_VLS_INIT_ZERO;
    struct bu_vls island_name = BU_VLS_INIT_ZERO;
    if (data->type == BREP) {
	type = BREP;
	subbrep_obj_name(data->type, data->id, rname, &island_name);
    }
    if ( data->type == COMB && BU_PTBL_LEN(data->children) > 0) {
	type = COMB;
	BU_LIST_INIT(&wcomb.l);
	subbrep_obj_name(data->type, data->id, rname, &island_name);
    }
    if (type == -1) {
	subbrep_obj_name(data->nucleus->type, data->nucleus->id, rname, &island_name);
    }

    switch (type) {
	case BREP:
	    if (!data->local_brep->IsValid()) {
		if (msgs) bu_vls_printf(msgs, "Warning - data->local_brep is not valid for %s\n", bu_vls_addr(&island_name));
	    }
	    mk_brep(wdbp, bu_vls_addr(&island_name), data->local_brep);
	    break;
	case COMB:
	    // TODO - need to reverse booleans here for a negative nucleus island...
	    bu_vls_trunc(&shoal_name, 0);
	    subbrep_obj_name(data->nucleus->type, data->nucleus->id, rname, &shoal_name);
	    if (!make_shoal(msgs, data->nucleus, wdbp, rname)) failed++;
	    (void)mk_addmember(bu_vls_addr(&shoal_name), &(wcomb.l), NULL, db_str2op(&(data->nucleus->params->bool_op)));
	    if (data->children && BU_PTBL_LEN(data->children) > 0) {
		for (unsigned int i = 0; i < BU_PTBL_LEN(data->children); i++) {
		    struct subbrep_shoal_data *d = (struct subbrep_shoal_data *)BU_PTBL_GET(data->children, i);
		    bu_vls_trunc(&shoal_name, 0);
		    subbrep_obj_name(d->type, d->id, rname, &shoal_name);
		    if (!make_shoal(msgs, d, wdbp, rname)) failed++;
		    (void)mk_addmember(bu_vls_addr(&shoal_name), &(wcomb.l), NULL, db_str2op(&(data->nucleus->params->bool_op)));

		}
	    }
	    mk_lcomb(wdbp, bu_vls_addr(&island_name), &wcomb, 0, NULL, NULL, NULL, 0);
	    break;
	default:
	    if (!make_shoal(msgs, data->nucleus, wdbp, rname)) failed++;
	    break;
    }

    if (!failed) {
	if (*bool_op == 'u' && pcomb) (void)mk_addmember(bu_vls_addr(&island_name), &(pcomb->l), NULL, db_str2op(bool_op));

	// Handle subtractions
	for (unsigned int i = 0; i < BU_PTBL_LEN(data->subtractions); i++) {
	    char sub = '-';
	    struct subbrep_island_data *n = (struct subbrep_island_data *)BU_PTBL_GET(data->subtractions, i);
	    struct bu_vls subtraction_name = BU_VLS_INIT_ZERO;
	    bu_log("subtraction found for %s: %s\n", bu_vls_addr(data->key), bu_vls_addr(n->key));
	    if (n->type == BREP || n->type == COMB) {
		// TODO - need unique ids for each island too for this to work.
		subbrep_obj_name(n->type, n->id, rname, &subtraction_name);
	    } else {
		subbrep_obj_name(n->nucleus->params->type, n->nucleus->params->id, rname, &subtraction_name);
	    }
	    bu_log("subtracting %s\n", bu_vls_addr(n->key));
	    if (pcomb) (void)mk_addmember(bu_vls_addr(&subtraction_name), &(pcomb->l), NULL, db_str2op(&sub));
	    bu_vls_free(&subtraction_name);
	}
    } else {
	if (msgs) bu_vls_printf(msgs, "shoal build failed\n");
    }
#if 1
    if (data->local_brep) {
	char un = 'u';
	unsigned char rgb[3];
	struct wmember bcomb;
	struct bu_vls bcomb_name = BU_VLS_INIT_ZERO;
	struct bu_vls brep_name = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&bcomb_name, "brep_obj_%d.r", data->id);
	BU_LIST_INIT(&bcomb.l);

	for (int i = 0; i < 3; ++i)
	    rgb[i] = static_cast<unsigned char>(255.0 * drand48() + 0.5);

	bu_vls_sprintf(&brep_name, "brep_obj_%d.s", data->id);
	mk_brep(wdbp, bu_vls_addr(&brep_name), data->local_brep);

	(void)mk_addmember(bu_vls_addr(&brep_name), &(bcomb.l), NULL, db_str2op((const char *)&un));
	mk_lcomb(wdbp, bu_vls_addr(&bcomb_name), &bcomb, 1, "plastic", "di=.8 sp=.2", rgb, 0);

	bu_vls_free(&brep_name);
	bu_vls_free(&bcomb_name);
    }
#endif


    bu_vls_free(&island_name);
    return 0;
}

/* return codes:
 * -1 get internal failure
 *  0 success
 *  1 not a brep
 *  2 not a valid brep
 */
int
brep_to_csg(struct ged *gedp, struct directory *dp, int UNUSED(verify))
{
    /* Unpack B-Rep */
    struct rt_db_internal intern;
    struct rt_brep_internal *brep_ip = NULL;
    struct rt_wdb *wdbp = gedp->ged_wdbp;
    RT_DB_INTERNAL_INIT(&intern)
    if (rt_db_get_internal(&intern, dp, wdbp->dbip, NULL, &rt_uniresource) < 0) {
	return -1;
    }
    bu_vls_printf(gedp->ged_result_str, "processing %s\n", dp->d_namep);
    if (intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a B-Rep - aborting\n", dp->d_namep);
	return 1;
    } else {
	brep_ip = (struct rt_brep_internal *)intern.idb_ptr;
    }
    RT_BREP_CK_MAGIC(brep_ip);
#if 1
    if (!rt_brep_valid(&intern, NULL)) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a valid B-Rep - aborting\n", dp->d_namep);
	return 2;
    }
#endif
    ON_Brep *brep = brep_ip->brep;

    struct wmember pcomb;
    struct bu_vls comb_name = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&comb_name, "csg_%s", dp->d_namep);
    BU_LIST_INIT(&pcomb.l);

    struct bu_ptbl *subbreps = find_subbreps(gedp->ged_result_str, brep);

    if (subbreps) {
	for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++) {
	    struct subbrep_island_data *sb = (struct subbrep_island_data *)BU_PTBL_GET(subbreps, i);
	    make_island(gedp->ged_result_str, sb, wdbp, bu_vls_addr(&comb_name), &pcomb);
	}
	for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++) {
	    // free islands;
	}

	// Only do a combination if the comb structure has more than one entry in the list.
	struct bu_list *olist;
	int comb_objs = 0;
	for (BU_LIST_FOR(olist, bu_list, &pcomb.l)) comb_objs++;
	if (comb_objs > 1) {
	    // This should probably be a region.
	    mk_lcomb(wdbp, bu_vls_addr(&comb_name), &pcomb, 0, NULL, NULL, NULL, 0);
	} else {
	    // TODO - Fix up name of first item in list to reflect top level naming.
	}
    }
    return 0;
}







/*************************************************/
/*               Tree walking, etc.              */
/*************************************************/

int comb_to_csg(struct ged *gedp, struct directory *dp, int verify);

int
brep_csg_conversion_tree(struct ged *gedp, const union tree *oldtree, union tree *newtree, int verify)
{
    int ret = 0;
    *newtree = *oldtree;
    switch (oldtree->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    /* convert right */
	    //bu_log("convert right\n");
	    newtree->tr_b.tb_right = new tree;
	    RT_TREE_INIT(newtree->tr_b.tb_right);
	    ret = brep_csg_conversion_tree(gedp, oldtree->tr_b.tb_right, newtree->tr_b.tb_right, verify);
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
	    //bu_log("convert left\n");
	    BU_ALLOC(newtree->tr_b.tb_left, union tree);
	    RT_TREE_INIT(newtree->tr_b.tb_left);
	    ret = brep_csg_conversion_tree(gedp, oldtree->tr_b.tb_left, newtree->tr_b.tb_left, verify);
#if 0
	    if (ret) {
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
	    //bu_log("have leaf: %s\n", oldtree->tr_l.tl_name);
	    //bu_log("checking for: %s\n", bu_vls_addr(&tmpname));
	    if (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&tmpname), LOOKUP_QUIET) == RT_DIR_NULL) {
		struct directory *dir = db_lookup(gedp->ged_wdbp->dbip, oldname, LOOKUP_QUIET);

		if (dir != RT_DIR_NULL) {
		    if (dir->d_flags & RT_DIR_COMB) {
			ret = comb_to_csg(gedp, dir, verify);
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
		    int brep_c = brep_to_csg(gedp, dir, verify);
		    int need_break = 0;
		    switch (brep_c) {
			case 0:
			    bu_vls_printf(gedp->ged_result_str, "processed brep %s.\n", bu_vls_addr(&tmpname));
			    bu_strlcpy(newtree->tr_l.tl_name, bu_vls_addr(&tmpname), strlen(bu_vls_addr(&tmpname))+1);
			    break;
			case 1:
			    bu_vls_printf(gedp->ged_result_str, "non brep solid %s.\n", bu_vls_addr(&tmpname));
			    bu_strlcpy(newtree->tr_l.tl_name, oldname, strlen(oldname)+1);
			    break;
			case 2:
			    bu_vls_printf(gedp->ged_result_str, "skipped invalid brep %s.\n", bu_vls_addr(&tmpname));
			    bu_strlcpy(newtree->tr_l.tl_name, oldname, strlen(oldname)+1);
			default:
			    bu_vls_free(&tmpname);
			    need_break = 1;
			    break;
		    }
		    if (need_break) break;
		} else {
		    bu_vls_printf(gedp->ged_result_str, "Cannot find %s.\n", oldname);
		    newtree = NULL;
		    ret = -1;
		}
	    } else {
		bu_vls_printf(gedp->ged_result_str, "%s already exists.\n", bu_vls_addr(&tmpname));
		bu_strlcpy(newtree->tr_l.tl_name, bu_vls_addr(&tmpname), strlen(bu_vls_addr(&tmpname))+1);
	    }
	    bu_vls_free(&tmpname);
	    break;
    }
    return 1;
}


int
comb_to_csg(struct ged *gedp, struct directory *dp, int verify)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb_internal = NULL;
    struct rt_wdb *wdbp = gedp->ged_wdbp;
    struct bu_vls comb_name = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&comb_name, "csg_%s", dp->d_namep);

    RT_DB_INTERNAL_INIT(&intern)

    if (rt_db_get_internal(&intern, dp, wdbp->dbip, NULL, &rt_uniresource) < 0) {
	return -1;
    }

    RT_CK_COMB(intern.idb_ptr);
    comb_internal = (struct rt_comb_internal *)intern.idb_ptr;

    if (comb_internal->tree == NULL) {
	// Empty tree
	(void)wdb_export(wdbp, bu_vls_addr(&comb_name), comb_internal, ID_COMBINATION, 1);
	return 0;
    }

    RT_CK_TREE(comb_internal->tree);
    union tree *oldtree = comb_internal->tree;
    struct rt_comb_internal *new_internal;

    BU_ALLOC(new_internal, struct rt_comb_internal);
    *new_internal = *comb_internal;
    BU_ALLOC(new_internal->tree, union tree);
    RT_TREE_INIT(new_internal->tree);

    union tree *newtree = new_internal->tree;

    (void)brep_csg_conversion_tree(gedp, oldtree, newtree, verify);
    (void)wdb_export(wdbp, bu_vls_addr(&comb_name), (void *)new_internal, ID_COMBINATION, 1);

    return 0;
}

extern "C" int
_ged_brep_to_csg(struct ged *gedp, const char *dp_name, int verify)
{
    struct rt_wdb *wdbp = gedp->ged_wdbp;
    struct directory *dp = db_lookup(wdbp->dbip, dp_name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) return GED_ERROR;

    if (dp->d_flags & RT_DIR_COMB) {
	return comb_to_csg(gedp, dp, verify) ? GED_ERROR : GED_OK;
    } else {
	return brep_to_csg(gedp, dp, verify) ? GED_ERROR : GED_OK;
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
