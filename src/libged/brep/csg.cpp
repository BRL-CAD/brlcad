/*                         C S G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libged/brep/csg.cpp
 *
 * Represent brep objects using CSG implicit solids and boolean
 * trees.
 *
 */

#include "common.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "vmath.h"
#include "bu/avs.h"
#include "bu/path.h"
#include "brep.h"
#include "wdb.h"
#include "analyze.h"
#include "ged.h"

/* TODO - this indicates something should be public API from libbrep... */
#include "../libbrep/shape_recognition/shape_recognition.h"

#define ptout(p)  p.x << " " << p.y << " " << p.z

#define INFORMATION_ATTRS_ON 0

HIDDEN void
set_attr_key(struct rt_wdb *wdbp, const char *obj_name, const char *key, int array_cnt, int *array)
{
    struct bu_vls val = BU_VLS_INIT_ZERO;
    struct bu_attribute_value_set avs;
    struct directory *dp;

    if (!wdbp || !obj_name || !key || !array) return;

    dp = db_lookup(wdbp->dbip, obj_name, LOOKUP_QUIET);

    if (dp == RT_DIR_NULL) return;

    set_key(&val, array_cnt, array);

    bu_avs_init_empty(&avs);

    if (db5_get_attributes(wdbp->dbip, &avs, dp)) return;

    (void)bu_avs_add(&avs, key, bu_vls_addr(&val));

#ifdef INFORMATION_ATTRS_ON
    (void)db5_replace_attributes(dp, &avs, wdbp->dbip);
#endif

    bu_avs_free(&avs);
    bu_vls_free(&val);
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
	case BREP:
	    bu_vls_printf(name, "%s-brep_%d.s", root, id);
	    break;
	default:
	    bu_vls_printf(name, "%s-%d.c", root, id);
	    break;
    }
}


HIDDEN int
subbrep_to_csg_arbn(struct bu_vls *msgs, struct csg_object_params *data, struct rt_wdb *wdbp, const char *pname)
{
    if (!msgs || !data || !wdbp || !pname) return 0;
    if (data->csg_type == ARBN) {
	/* Make the arbn, using the data key for a unique name */
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data->csg_type, data->csg_id, pname, &prim_name);
	if (mk_arbn(wdbp, bu_vls_addr(&prim_name), data->plane_cnt, data->planes)) {
	    if (msgs) bu_vls_printf(msgs, "mk_arbn failed for %s\n", bu_vls_addr(&prim_name));
	} else {
	    set_attr_key(wdbp, bu_vls_addr(&prim_name), "loops", data->s->shoal_loops_cnt, data->s->shoal_loops);
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
    if (data->csg_type == PLANAR_VOLUME) {
	/* Make the bot, using the data key for a unique name */
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data->csg_type, data->csg_id, pname, &prim_name);
	if (mk_bot(wdbp, bu_vls_addr(&prim_name), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, data->csg_vert_cnt, data->csg_face_cnt, (fastf_t *)data->csg_verts, data->csg_faces, (fastf_t *)NULL, (struct bu_bitv *)NULL)) {
	    if (msgs) bu_vls_printf(msgs, "mk_bot failed for %s\n", bu_vls_addr(&prim_name));
	} else {
	    set_attr_key(wdbp, bu_vls_addr(&prim_name), "loops", data->s->shoal_loops_cnt, data->s->shoal_loops);
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
    if (data->csg_type == CYLINDER) {
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data->csg_type, data->csg_id, pname, &prim_name);
	if (mk_rcc(wdbp, bu_vls_addr(&prim_name), data->origin, data->hv, data->radius)) {
	    if (msgs) bu_vls_printf(msgs, "mk_rcc failed for %s\n", bu_vls_addr(&prim_name));
	} else {
	    set_attr_key(wdbp, bu_vls_addr(&prim_name), "loops", data->s->shoal_loops_cnt, data->s->shoal_loops);
	}
	bu_vls_free(&prim_name);
	return 1;
    }
    return 0;
}

HIDDEN int
subbrep_to_csg_cone(struct bu_vls *msgs, struct csg_object_params *data, struct rt_wdb *wdbp, const char *pname)
{
    if (!msgs || !data || !wdbp || !pname) return 0;
    if (data->csg_type == CONE) {
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data->csg_type, data->csg_id, pname, &prim_name);
	if (mk_cone(wdbp, bu_vls_addr(&prim_name), data->origin, data->hv, data->height, data->radius, data->r2)) {
	    if (msgs) bu_vls_printf(msgs, "mk_trc failed for %s\n", bu_vls_addr(&prim_name));
	} else {
	    set_attr_key(wdbp, bu_vls_addr(&prim_name), "loops", data->s->shoal_loops_cnt, data->s->shoal_loops);
	}
	bu_vls_free(&prim_name);
	return 1;
    }
    return 0;
}

HIDDEN int
subbrep_to_csg_sph(struct bu_vls *msgs, struct csg_object_params *data, struct rt_wdb *wdbp, const char *pname)
{
    if (!msgs || !data || !wdbp || !pname) return 0;
    if (data->csg_type == SPHERE) {
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data->csg_type, data->csg_id, pname, &prim_name);
	if (mk_sph(wdbp, bu_vls_addr(&prim_name), data->origin, data->radius)) {
	    if (msgs) bu_vls_printf(msgs, "mk_sph failed for %s\n", bu_vls_addr(&prim_name));
	} else {
	    set_attr_key(wdbp, bu_vls_addr(&prim_name), "loops", data->s->shoal_loops_cnt, data->s->shoal_loops);
	}
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
    subbrep_obj_name(data->csg_type, data->csg_id, pname, &prim_name);
    struct directory *dp = db_lookup(wdbp->dbip, bu_vls_addr(&prim_name), LOOKUP_QUIET);
    // Don't recreate it
    if (dp != RT_DIR_NULL) {
	bu_log("already made %s\n", bu_vls_addr(data->obj_name));
	return;
    }
#endif

    switch (data->csg_type) {
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
	    subbrep_to_csg_cone(msgs, data, wdbp, pname);
	    break;
	case SPHERE:
	    subbrep_to_csg_sph(msgs, data, wdbp, pname);
	    break;
	case ELLIPSOID:
	    break;
	case TORUS:
	    break;
	default:
	    break;
    }
}

#define BOOL_RESOLVE(_a, _b) (_b == '+') ? isect : ((_a == '-' && _b == '-') || (_a == 'u' && _b == 'u')) ? un : sub

HIDDEN int
make_shoal(struct bu_vls *msgs, struct subbrep_shoal_data *data, struct rt_wdb *wdbp, const char *rname)
{
    const char *un = "u\0";
    const char *sub = "-\0";
    const char *isect = "+\0";

    struct wmember wcomb;
    struct bu_vls prim_name = BU_VLS_INIT_ZERO;
    struct bu_vls comb_name = BU_VLS_INIT_ZERO;
    if (!data || !data->params) {
	if (msgs) bu_vls_printf(msgs, "Error! invalid shoal.\n");
	return 0;
    }

    //struct bu_vls key = BU_VLS_INIT_ZERO;
    //set_key(&key, data->shoal_loops_cnt, data->shoal_loops);
    //bu_log("Processing shoal %s from island %s\n", bu_vls_addr(&key), bu_vls_addr(data->i->key));

    if (data->shoal_type == COMB) {
	BU_LIST_INIT(&wcomb.l);
	subbrep_obj_name(data->shoal_type, data->shoal_id, rname, &comb_name);
	//bu_log("Created %s\n", bu_vls_addr(&comb_name));
	csg_obj_process(msgs, data->params, wdbp, rname);
	subbrep_obj_name(data->params->csg_type, data->params->csg_id, rname, &prim_name);
	//bu_log("  %c %s\n", data->params->bool_op, bu_vls_addr(&prim_name));
	(void)mk_addmember(bu_vls_addr(&prim_name), &(wcomb.l), NULL, db_str2op(&(data->params->bool_op)));
	for (unsigned int i = 0; i < BU_PTBL_LEN(data->shoal_children); i++) {
	    struct csg_object_params *c = (struct csg_object_params *)BU_PTBL_GET(data->shoal_children, i);
	    const char *bool_op = BOOL_RESOLVE(data->params->bool_op, c->bool_op);
	    csg_obj_process(msgs, c, wdbp, rname);
	    bu_vls_trunc(&prim_name, 0);
	    subbrep_obj_name(c->csg_type, c->csg_id, rname, &prim_name);
	    //bu_log("     %c(%c) %s\n", c->bool_op, *bool_op, bu_vls_addr(&prim_name));
	    (void)mk_addmember(bu_vls_addr(&prim_name), &(wcomb.l), NULL, db_str2op(bool_op));
	}
	mk_lcomb(wdbp, bu_vls_addr(&comb_name), &wcomb, 0, NULL, NULL, NULL, 0);
	set_attr_key(wdbp, bu_vls_addr(&comb_name), "loops", data->shoal_loops_cnt, data->shoal_loops);
    } else {
	subbrep_obj_name(data->params->csg_type, data->params->csg_id, rname, &prim_name);
	csg_obj_process(msgs, data->params, wdbp, rname);
	//bu_log("Created %s\n", bu_vls_addr(&prim_name));
    }
    return 1;
}

HIDDEN int
make_island(struct bu_vls *msgs, struct subbrep_island_data *data, struct rt_wdb *wdbp, const char *rname, struct wmember *pcomb)
{
    struct wmember icomb;
    struct wmember ucomb;
    struct wmember scomb;

    int failed = 0;
    const char *un = "u\0";
    const char *sub = "-\0";
    const char *isect = "+\0"; // UNUSED here, for BOOL_RESOLVE macro.

    char *n_bool_op;
    if (data->island_type == BREP) {
	n_bool_op = &(data->local_brep_bool_op);
    } else {
	n_bool_op = &(data->nucleus->params->bool_op);
    }
    struct bu_vls island_name = BU_VLS_INIT_ZERO;
    subbrep_obj_name(-1, data->island_id, rname, &island_name);

#if 0
    if (*n_bool_op == 'u') {
	bu_log("Processing island %s\n", bu_vls_addr(&island_name));
    } else {
	bu_log("Making negative island %s\n", bu_vls_addr(&island_name));
    }
#endif

    struct bu_vls shoal_name = BU_VLS_INIT_ZERO;
    struct bu_vls union_name = BU_VLS_INIT_ZERO;
    struct bu_vls sub_name = BU_VLS_INIT_ZERO;

    BU_LIST_INIT(&icomb.l);
    BU_LIST_INIT(&ucomb.l);
    bu_vls_sprintf(&union_name, "%s-unions", bu_vls_addr(&island_name));
    BU_LIST_INIT(&scomb.l);
    bu_vls_sprintf(&sub_name, "%s-subtractions", bu_vls_addr(&island_name));

    bu_vls_trunc(&shoal_name, 0);
    if (data->island_type == BREP) {
	subbrep_obj_name(data->island_type, data->island_id, rname, &shoal_name);
	mk_brep(wdbp, bu_vls_addr(&shoal_name), (void *)(data->local_brep));
	n_bool_op = &(data->local_brep_bool_op);
    } else {
	subbrep_obj_name(data->nucleus->shoal_type, data->nucleus->shoal_id, rname, &shoal_name);
	if (!make_shoal(msgs, data->nucleus, wdbp, rname)) failed++;
    }

    // In a comb, the first element is always unioned.  The nucleus bool op is applied
    // to the overall shoal in the pcomb assembly.
    (void)mk_addmember(bu_vls_addr(&shoal_name), &(ucomb.l), NULL, db_str2op(un));
    const char *bool_op;
    int union_shoal_cnt = 1;
    int subtraction_shoal_cnt = 0;

    // Find and handle union shoals first.
    for (unsigned int i = 0; i < BU_PTBL_LEN(data->island_children); i++) {
	struct subbrep_shoal_data *d = (struct subbrep_shoal_data *)BU_PTBL_GET(data->island_children, i);
	bool_op = BOOL_RESOLVE(*n_bool_op, d->params->bool_op);
	if (*bool_op == 'u') {
	    bu_vls_trunc(&shoal_name, 0);
	    subbrep_obj_name(d->shoal_type, d->shoal_id, rname, &shoal_name);
	    //if (*n_bool_op == 'u') {
	    //  bu_log("  unioning: %s: %s\n", bu_vls_addr(&island_name), bu_vls_addr(&shoal_name));
	    //}
	    if (!make_shoal(msgs, d, wdbp, rname)) failed++;
	    (void)mk_addmember(bu_vls_addr(&shoal_name), &(ucomb.l), NULL, db_str2op(bool_op));
	    union_shoal_cnt++;
	}
    }

    if (union_shoal_cnt == 1) {
	bu_vls_sprintf(&union_name, "%s", bu_vls_addr(&shoal_name));
	//bu_log("single union name: %s\n", bu_vls_addr(&union_name));
    }

    // Have unions, get subtractions.
    for (unsigned int i = 0; i < BU_PTBL_LEN(data->island_children); i++) {
	struct subbrep_shoal_data *d = (struct subbrep_shoal_data *)BU_PTBL_GET(data->island_children, i);
	bool_op = BOOL_RESOLVE(*n_bool_op, d->params->bool_op);
	if (*bool_op == '-') {
	    bu_vls_trunc(&shoal_name, 0);
	    subbrep_obj_name(d->shoal_type, d->shoal_id, rname, &shoal_name);
	    //if (*n_bool_op == 'u') {
		//bu_log("  subtracting: %s: %s\n", bu_vls_addr(&island_name), bu_vls_addr(&shoal_name));
	    //}
	    if (!make_shoal(msgs, d, wdbp, rname)) failed++;
	    (void)mk_addmember(bu_vls_addr(&shoal_name), &(scomb.l), NULL, db_str2op(un));
	    subtraction_shoal_cnt++;
	}
    }
    // Handle subtractions
    for (unsigned int i = 0; i < BU_PTBL_LEN(data->subtractions); i++) {
	struct subbrep_island_data *n = (struct subbrep_island_data *)BU_PTBL_GET(data->subtractions, i);
	struct bu_vls subtraction_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(-1, n->island_id, rname, &subtraction_name);
	//if (*n_bool_op == 'u') {
	//  bu_log("  subtraction found for %s: %s\n", bu_vls_addr(&island_name), bu_vls_addr(&subtraction_name));
	//}
	(void)mk_addmember(bu_vls_addr(&subtraction_name), &(scomb.l), NULL, db_str2op(un));
	bu_vls_free(&subtraction_name);
	subtraction_shoal_cnt++;
    }

    if (union_shoal_cnt > 1) {
	mk_lcomb(wdbp, bu_vls_addr(&union_name), &ucomb, 0, NULL, NULL, NULL, 0);
    }
    (void)mk_addmember(bu_vls_addr(&union_name), &(icomb.l), NULL, db_str2op(un));
    if (subtraction_shoal_cnt) {
	mk_lcomb(wdbp, bu_vls_addr(&sub_name), &scomb, 0, NULL, NULL, NULL, 0);
	(void)mk_addmember(bu_vls_addr(&sub_name), &(icomb.l), NULL, db_str2op(sub));
    }
    mk_lcomb(wdbp, bu_vls_addr(&island_name), &icomb, 0, NULL, NULL, NULL, 0);
    set_attr_key(wdbp, bu_vls_addr(&island_name), "loops", data->island_loops_cnt, data->island_loops);
    set_attr_key(wdbp, bu_vls_addr(&island_name), "faces", data->island_faces_cnt, data->island_faces);

    if (*n_bool_op == 'u')
	(void)mk_addmember(bu_vls_addr(&island_name), &(pcomb->l), NULL, db_str2op(n_bool_op));

    // Debugging B-Reps - generates a B-Rep object for each island
#if 0
    if (data->local_brep) {
	unsigned char rgb[3];
	struct wmember bcomb;
	struct bu_vls bcomb_name = BU_VLS_INIT_ZERO;
	struct bu_vls brep_name = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&bcomb_name, "%s-brep_obj_%d.r", rname, data->island_id);
	BU_LIST_INIT(&bcomb.l);

	if (*n_bool_op == 'u') {
	    rgb[0] = static_cast<unsigned char>(0);
	    rgb[1] = static_cast<unsigned char>(0);
	    rgb[2] = static_cast<unsigned char>(255.0);
	} else {
	    rgb[0] = static_cast<unsigned char>(255.0);
	    rgb[1] = static_cast<unsigned char>(0);
	    rgb[2] = static_cast<unsigned char>(0);
	}
	//for (int i = 0; i < 3; ++i)
	//    rgb[i] = static_cast<unsigned char>(255.0 * drand48() + 0.5);

	bu_vls_sprintf(&brep_name, "%s-brep_obj_%d.s", rname, data->island_id);
	mk_brep(wdbp, bu_vls_addr(&brep_name), (void *)(data->local_brep));

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
_obj_brep_to_csg(struct ged *gedp, struct bu_vls *log, struct bu_attribute_value_set *UNUSED(ito), struct directory *dp, int verify, struct bu_vls *retname)
{
    /* Unpack B-Rep */
    struct rt_db_internal intern;
    struct rt_brep_internal *brep_ip = NULL;
    struct rt_wdb *wdbp = gedp->ged_wdbp;
    RT_DB_INTERNAL_INIT(&intern)
    if (rt_db_get_internal(&intern, dp, wdbp->dbip, NULL, &rt_uniresource) < 0) {
	return -1;
    }
    if (intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(log, "%s is not a B-Rep - aborting\n", dp->d_namep);
	return 1;
    } else {
	brep_ip = (struct rt_brep_internal *)intern.idb_ptr;
    }
    RT_BREP_CK_MAGIC(brep_ip);
#if 0
    if (!rt_brep_valid(&intern, NULL)) {
	bu_vls_printf(log, "%s is not a valid B-Rep - aborting\n", dp->d_namep);
	return 2;
    }
#endif
    ON_Brep *brep = brep_ip->brep;

    struct wmember pcomb;
    struct bu_vls core_name = BU_VLS_INIT_ZERO;
    struct bu_vls comb_name = BU_VLS_INIT_ZERO;
    struct bu_vls root_name = BU_VLS_INIT_ZERO;
    bu_path_component(&core_name, dp->d_namep, BU_PATH_BASENAME_EXTLESS);
    bu_vls_sprintf(&root_name, "%s-csg", bu_vls_addr(&core_name));
    bu_vls_sprintf(&comb_name, "csg_%s.c", bu_vls_addr(&core_name));
    if (retname) bu_vls_sprintf(retname, "%s", bu_vls_addr(&comb_name));

    // Only do this if we haven't already done it - tree walking may
    // result in multiple references to a single object
    if (db_lookup(gedp->dbip, bu_vls_addr(&comb_name), LOOKUP_QUIET) == RT_DIR_NULL) {
	bu_vls_printf(log, "Converting %s to %s\n", dp->d_namep, bu_vls_addr(&comb_name));

	BU_LIST_INIT(&pcomb.l);

	struct bu_ptbl *subbreps = brep_to_csg(log, brep);

	if (subbreps) {
	    int have_non_breps = 0;
	    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++) {
		struct subbrep_island_data *sb = (struct subbrep_island_data *)BU_PTBL_GET(subbreps, i);
		if (sb->island_type != BREP) have_non_breps++;
	    }
	    if (!have_non_breps) return 2;

	    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++) {
		struct subbrep_island_data *sb = (struct subbrep_island_data *)BU_PTBL_GET(subbreps, i);
		make_island(log, sb, wdbp, bu_vls_addr(&root_name), &pcomb);
	    }
	    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++) {
		// free islands;
	    }

	    // Only do a combination if the comb structure has more than one entry in the list.
	    struct bu_list *olist;
	    int comb_objs = 0;
	    for (BU_LIST_FOR(olist, bu_list, &pcomb.l)) comb_objs++;
	    if (comb_objs > 1) {
		// We're not setting the region flag here in case there is a hierarchy above us that
		// takes care of it.  TODO - support knowing whether that's true and doing the right thing.
		mk_lcomb(wdbp, bu_vls_addr(&comb_name), &pcomb, 0, NULL, NULL, NULL, 0);
	    } else {
		// TODO - Fix up name of first item in list to reflect top level naming to
		// avoid an unnecessary level of hierarchy.
		//bu_log("only one level... - %s\n", ((struct wmember *)pcomb.l.forw)->wm_name);
		/*
		int ac = 3;
		const char **av = (const char **)bu_calloc(4, sizeof(char *), "killtree argv");
		av[0] = "mv";
		av[1] = ((struct wmember *)pcomb.l.forw)->wm_name;
		av[2] = bu_vls_addr(&comb_name);
		av[3] = (char *)0;
		(void)ged_move(gedp, ac, av);
		bu_free(av, "free av array");
		bu_vls_sprintf(&comb_name, "%s", ((struct wmember *)pcomb.l.forw)->wm_name);
		*/
		mk_lcomb(wdbp, bu_vls_addr(&comb_name), &pcomb, 0, NULL, NULL, NULL, 0);
	    }

	    // Verify that the resulting csg tree and the original B-Rep pass a difference test.
	    if (verify) {
		ON_BoundingBox bbox;
		struct bn_tol tol = BG_TOL_INIT;
		brep->GetBoundingBox(bbox);
		tol.dist = (bbox.Diagonal().Length() / 100.0);
		bu_vls_printf(log, "Analyzing %s csg conversion, tol %f...\n", dp->d_namep, tol.dist);
		if (analyze_raydiff(NULL, gedp->dbip, dp->d_namep, bu_vls_addr(&comb_name), &tol, 1)) {
		    /* remove generated tree if debugging flag isn't passed - not valid */
		    int ac = 3;
		    const char **av = (const char **)bu_calloc(4, sizeof(char *), "killtree argv");
		    av[0] = "killtree";
		    av[1] = "-f";
		    av[2] = bu_vls_addr(&comb_name);
		    av[3] = (char *)0;
		    (void)ged_killtree(gedp, ac, av);
		    bu_free(av, "free av array");
		    bu_vls_printf(log, "Error: %s did not pass diff test at tol %f, rejecting\n", bu_vls_addr(&comb_name), tol.dist);
		    return 2;
		}
	    }
	} else {
	    return 2;
	}
    } else {
	bu_vls_printf(log, "Conversion object %s for %s already exists, skipping.\n", bu_vls_addr(&comb_name), dp->d_namep);
    }
    return 0;
}







/*************************************************/
/*               Tree walking, etc.              */
/*************************************************/

int comb_to_csg(struct ged *gedp, struct bu_vls *log, struct bu_attribute_value_set *ito, struct directory *dp, int verify);

static int
brep_csg_conversion_tree(struct ged *gedp, struct bu_vls *log, struct bu_attribute_value_set *ito, const union tree *oldtree, union tree *newtree, int verify)
{
    int ret = 0;
    int brep_c;
    struct bu_vls tmpname = BU_VLS_INIT_ZERO;
    struct bu_vls newname = BU_VLS_INIT_ZERO;
    char *oldname = NULL;
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
	    ret |= brep_csg_conversion_tree(gedp, log, ito, oldtree->tr_b.tb_right, newtree->tr_b.tb_right, verify);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    /* convert left */
	    //bu_log("convert left\n");
	    BU_ALLOC(newtree->tr_b.tb_left, union tree);
	    RT_TREE_INIT(newtree->tr_b.tb_left);
	    ret |= brep_csg_conversion_tree(gedp, log, ito, oldtree->tr_b.tb_left, newtree->tr_b.tb_left, verify);
	    break;
	case OP_DB_LEAF:
	    oldname = oldtree->tr_l.tl_name;
	    bu_vls_sprintf(&tmpname, "csg_%s", oldname);
	    if (db_lookup(gedp->dbip, bu_vls_addr(&tmpname), LOOKUP_QUIET) == RT_DIR_NULL) {
		struct directory *dir = db_lookup(gedp->dbip, oldname, LOOKUP_QUIET);

		if (dir != RT_DIR_NULL) {
		    if (dir->d_flags & RT_DIR_COMB) {
			ret = comb_to_csg(gedp, log, ito, dir, verify);
			if (!ret) {
			    newtree->tr_l.tl_name = (char*)bu_malloc(strlen(bu_vls_addr(&tmpname))+1, "char");
			    bu_strlcpy(newtree->tr_l.tl_name, bu_vls_addr(&tmpname), strlen(bu_vls_addr(&tmpname))+1);
			}
			bu_vls_free(&tmpname);
		    } else {
			// It's a primitive. If it's a b-rep object, convert it. Otherwise,
			// just duplicate it. Might need better error codes from _obj_brep_to_csg for this...
			brep_c = _obj_brep_to_csg(gedp, log, ito, dir, verify, &newname);
			switch (brep_c) {
			    case 0:
				bu_vls_printf(log, "processed brep %s.\n", bu_vls_addr(&newname));
				newtree->tr_l.tl_name = (char*)bu_malloc(strlen(bu_vls_addr(&newname))+1, "char");
				bu_strlcpy(newtree->tr_l.tl_name, bu_vls_addr(&newname), strlen(bu_vls_addr(&newname))+1);
				bu_vls_free(&newname);
				break;
			    case 1:
				bu_vls_printf(log, "non brep solid %s.\n", bu_vls_addr(&tmpname));
				newtree->tr_l.tl_name = (char*)bu_malloc(strlen(bu_vls_addr(&tmpname))+1, "char");
				bu_strlcpy(newtree->tr_l.tl_name, oldname, strlen(oldname)+1);
				break;
			    case 2:
				bu_vls_printf(log, "unconverted brep %s.\n", bu_vls_addr(&tmpname));
				newtree->tr_l.tl_name = (char*)bu_malloc(strlen(bu_vls_addr(&tmpname))+1, "char");
				bu_strlcpy(newtree->tr_l.tl_name, oldname, strlen(oldname)+1);
				break;
			    default:
				bu_vls_printf(log, "what?? %s.\n", bu_vls_addr(&tmpname));
				break;
			}
		    }
		} else {
		    bu_vls_printf(log, "Cannot find %s.\n", oldname);
		    newtree = NULL;
		    ret |= GED_ERROR;
		}
	    } else {
		bu_vls_printf(log, "%s already exists.\n", bu_vls_addr(&tmpname));
		newtree->tr_l.tl_name = (char*)bu_malloc(strlen(bu_vls_addr(&tmpname))+1, "char");
		bu_strlcpy(newtree->tr_l.tl_name, bu_vls_addr(&tmpname), strlen(bu_vls_addr(&tmpname))+1);
	    }
	    bu_vls_free(&tmpname);
	    break;
	default:
	    bu_log("huh??\n");
	    break;
    }

    return ret;
}


int
comb_to_csg(struct ged *gedp, struct bu_vls *log, struct bu_attribute_value_set *ito, struct directory *dp, int verify)
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

    if (brep_csg_conversion_tree(gedp, log, ito, oldtree, newtree, verify) & GED_ERROR)
	bu_log("Error (brep/csg.cpp:%d) brep_csg_conversion_tree\n", __LINE__);

    (void)wdb_export(wdbp, bu_vls_addr(&comb_name), (void *)new_internal, ID_COMBINATION, 1);

    return 0;
}

extern "C"
int _ged_brep_to_csg(struct ged *gedp, const char *dp_name, int verify)
{
    struct bu_attribute_value_set ito = BU_AVS_INIT_ZERO; /* islands to objects */
    int ret = 0;
    struct bu_vls log = BU_VLS_INIT_ZERO;
    struct rt_wdb *wdbp = gedp->ged_wdbp;
    struct directory *dp = db_lookup(wdbp->dbip, dp_name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) return GED_ERROR;

    if (dp->d_flags & RT_DIR_COMB) {
	ret = comb_to_csg(gedp, &log, &ito, dp, verify) ? GED_ERROR : GED_OK;
    } else {
	ret = _obj_brep_to_csg(gedp, &log, &ito, dp, verify, NULL) ? GED_ERROR : GED_OK;
    }

    bu_vls_sprintf(gedp->ged_result_str, "%s", bu_vls_addr(&log));
    bu_vls_free(&log);
    return ret;
}
// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
