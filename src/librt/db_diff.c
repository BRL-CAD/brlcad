/*                        D B _ D I F F . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file db_diff.c
 *
 * Routines to determine the differences between two BRL-CAD databases.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include "bio.h"

#include "tcl.h"

#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "rt/db_diff.h"

/* TODO - there has to be a better way to do this.  Seems like overkill to define
 * a functab function, but this is way too fragile */
HIDDEN int
rt_intern_struct_size(int type) {
    if (type >= ID_MAXIMUM) return 0;
    switch (type) {
	case  ID_NULL       :/**< @brief Unused */
	    return 0;
	    break;
	case  ID_TOR        :/**< @brief Toroid */
	    return sizeof(struct rt_tor_internal);
	    break;
	case  ID_TGC        :/**< @brief Generalized Truncated General Cone */
	    return sizeof(struct rt_tgc_internal);
	    break;
	case  ID_ELL        :/**< @brief Ellipsoid */
	    return sizeof(struct rt_ell_internal);
	    break;
	case  ID_ARB8       :/**< @brief Generalized ARB.  V + 7 vectors */
	    return sizeof(struct rt_arb_internal);
	    break;
	case  ID_ARS        :/**< @brief ARS */
	    return sizeof(struct rt_ars_internal);
	    break;
	case  ID_HALF       :/**< @brief Half-space */
	    return sizeof(struct rt_half_internal);
	    break;
	case  ID_REC        :/**< @brief Right Elliptical Cylinder [TGC special] */
	    return sizeof(struct rt_tgc_internal);
	    break;
	case  ID_POLY       :/**< @brief Polygonal faceted object */
	    return sizeof(struct rt_pg_face_internal);
	    break;
	case  ID_BSPLINE    :/**< @brief B-spline object */
	    return sizeof(struct rt_nurb_internal);
	    break;
	case  ID_SPH        :/**< @brief Sphere */
	    return sizeof(struct rt_ell_internal);
	    break;
	case  ID_NMG        :/**< @brief n-Manifold Geometry solid */
	    return sizeof(struct model);
	    break;
	case  ID_EBM        :/**< @brief Extruded bitmap solid */
	    return sizeof(struct rt_ebm_internal);
	    break;
	case  ID_VOL        :/**< @brief 3-D Volume */
	    return sizeof(struct rt_vol_internal);
	    break;
	case  ID_ARBN       :/**< @brief ARB with N faces */
	    return sizeof(struct rt_arbn_internal);
	    break;
	case  ID_PIPE       :/**< @brief Pipe (wire) solid */
	    return sizeof(struct rt_pipe_internal);
	    break;
	case  ID_PARTICLE   :/**< @brief Particle system solid */
	    return sizeof(struct rt_part_internal);
	    break;
	case  ID_RPC        :/**< @brief Right Parabolic Cylinder  */
	    return sizeof(struct rt_rpc_internal);
	    break;
	case  ID_RHC        :/**< @brief Right Hyperbolic Cylinder  */
	    return sizeof(struct rt_rhc_internal);
	    break;
	case  ID_EPA        :/**< @brief Elliptical Paraboloid  */
	    return sizeof(struct rt_epa_internal);
	    break;
	case  ID_EHY        :/**< @brief Elliptical Hyperboloid  */
	    return sizeof(struct rt_ehy_internal);
	    break;
	case  ID_ETO        :/**< @brief Elliptical Torus  */
	    return sizeof(struct rt_eto_internal);
	    break;
	case  ID_GRIP       :/**< @brief Pseudo Solid Grip */
	    return sizeof(struct rt_grip_internal);
	    break;
	case  ID_JOINT      :/**< @brief Pseudo Solid/Region Joint */
	    return 0;
	    break;
	case  ID_HF         :/**< @brief Height Field */
	    return sizeof(struct rt_hf_internal);
	    break;
	case  ID_DSP        :/**< @brief Displacement map */
	    return sizeof(struct rt_dsp_internal);
	    break;
	case  ID_SKETCH     :/**< @brief 2D sketch */
	    return sizeof(struct rt_sketch_internal);
	    break;
	case  ID_EXTRUDE    :/**< @brief Solid of extrusion */
	    return sizeof(struct rt_extrude_internal);
	    break;
	case  ID_SUBMODEL   :/**< @brief Instanced submodel */
	    return sizeof(struct rt_submodel_internal);
	    break;
	case  ID_CLINE      :/**< @brief FASTGEN4 CLINE solid */
	    return sizeof(struct rt_cline_internal);
	    break;
	case  ID_BOT        :/**< @brief Bag o' triangles */
	    return sizeof(struct rt_bot_internal);
	    break;
	case  ID_COMBINATION:/**< @brief Combination Record */
	    return sizeof(struct rt_comb_internal);
	    break;
	case  ID_BINUNIF    :/**< @brief Uniform-array binary */
	    return sizeof(struct rt_binunif_internal);
	    break;
	case  ID_CONSTRAINT :/**< @brief Constraint object */
	    return sizeof(struct rt_constraint_internal);
	    break;
	case  ID_SUPERELL   :/**< @brief Superquadratic ellipsoid */
	    return sizeof(struct rt_superell_internal);
	    break;
	case  ID_METABALL   :/**< @brief Metaball */
	    return sizeof(struct rt_metaball_internal);
	    break;
	case  ID_BREP       :/**< @brief B-rep object */
	    return sizeof(struct rt_brep_internal);
	    break;
	case  ID_HYP        :/**< @brief Hyperboloid of one sheet */
	    return sizeof(struct rt_hyp_internal);
	    break;
	case  ID_REVOLVE    :/**< @brief Solid of Revolution */
	    return sizeof(struct rt_revolve_internal);
	    break;
	case  ID_PNTS       :/**< @brief Collection of Points */
	    return sizeof(struct rt_pnts_internal);
	    break;
	case  ID_ANNOTATION :/**< @brief Annotation */
	    return sizeof(struct rt_annotation_internal);
	    break;
	case  ID_HRT        :/**< @brief Heart */
	    return sizeof(struct rt_hrt_internal);
	    break;
	default:
	    return 0;
	    break;
    }
    return 0;
}

HIDDEN int
db_diff_external(const struct bu_external *ext1, const struct bu_external *ext2)
{
    if (ext1->ext_nbytes != ext2->ext_nbytes) {
	return 1;
    }
    if (memcmp((void *)ext1->ext_buf, (void *)ext2->ext_buf, ext1->ext_nbytes)) {
	return 1;
    }
    return 0;
}

int
db_diff(const struct db_i *dbip1,
	const struct db_i *dbip2,
	int (*add_func)(const struct db_i *left, const struct db_i *right, const struct directory *added, void *data),
	int (*del_func)(const struct db_i *left, const struct db_i *right, const struct directory *removed, void *data),
	int (*chgd_func)(const struct db_i *left, const struct db_i *right, const struct directory *before, const struct directory *after, void *data),
	int (*unch_func)(const struct db_i *left, const struct db_i *right, const struct directory *unchanged, void *data),
	void *client_data)
{
    int ret = 0;
    int error = 0;
    struct directory *dp1, *dp2;
    struct bu_external ext1, ext2;

    /* look at all objects in this database */
    FOR_ALL_DIRECTORY_START(dp1, dbip1) {
	int this_diff = 0;
#if 0
	/* skip the _GLOBAL object for now - need to deal with this, however */
	if (dp1->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	    continue;
	}
#endif
	/* check if this object exists in the other database */
	if ((dp2 = db_lookup(dbip2, dp1->d_namep, 0)) == RT_DIR_NULL) {
	    this_diff++;
	    if (del_func && del_func(dbip1, dbip2, dp1, client_data)) error--;
	    continue;
	}

	/* Check if the objects differ */
	if (db_get_external(&ext1, dp1, dbip1) || db_get_external(&ext2, dp2, dbip2)) {
	    bu_log("Error getting bu_external form when comparing %s and %s\n", dp1->d_namep, dp2->d_namep);
	    error--;
	    continue;
	}

	if (db_diff_external(&ext1, &ext2)) {
	    this_diff++;
	    if (chgd_func && chgd_func(dbip1, dbip2, dp1, dp2, client_data)) error--;
	} else {
	    if (unch_func && unch_func(dbip1, dbip2, dp1, client_data)) error--;
	}

	ret += this_diff;

    } FOR_ALL_DIRECTORY_END;

    /* now look for objects in the other database that aren't here */
    FOR_ALL_DIRECTORY_START(dp2, dbip2) {
	int this_diff = 0;

#if 0
	/* skip the _GLOBAL object for now - need to deal with this, however */
	if (dp2->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	    continue;
	}
#endif

	/* check if this object exists in the other database */
	if ((dp1 = db_lookup(dbip1, dp2->d_namep, 0)) == RT_DIR_NULL) {
	    this_diff++;
	    if (add_func && add_func(dbip1, dbip2, dp2, client_data)) error--;
	}

	ret += this_diff;

    } FOR_ALL_DIRECTORY_END;

    if (!error) {
    	return ret;
    } else {
	return error;
    }
}

int
db_diff3(const struct db_i *left,
	const struct db_i *ancestor,
	const struct db_i *right,
	int (*add_func)(const struct db_i *left_dbip, const struct db_i *ancestor_dbip, const struct db_i *right_dbip, const struct directory *left, const struct directory *ancestor, const struct directory *right, void *data),
	int (*del_func)(const struct db_i *left_dbip, const struct db_i *ancestor_dbip, const struct db_i *right_dbip, const struct directory *left, const struct directory *ancestor, const struct directory *right, void *data),
	int (*chgd_func)(const struct db_i *left_dbip, const struct db_i *ancestor_dbip, const struct db_i *right_dbip, const struct directory *left, const struct directory *ancestor, const struct directory *right, void *data),
	int (*unchgd_func)(const struct db_i *left_dbip, const struct db_i *ancestor_dbip, const struct db_i *right_dbip, const struct directory *left, const struct directory *ancestor, const struct directory *right, void *data),
	void *client_data)
{
    int has_diff = 0;
    int error = 0;
    struct directory *dp_ancestor, *dp_left, *dp_right;

    /* Step 1: look at all objects in the ancestor database */
    FOR_ALL_DIRECTORY_START(dp_ancestor, ancestor) {
	struct bu_external ext_ancestor, ext_left, ext_right;
#if 0
	/* skip the _GLOBAL object for now - need to deal with this, however */
	if (dp_ancestor->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	    continue;
	}
#endif
	dp_left = db_lookup(left, dp_ancestor->d_namep, 0);
	dp_right = db_lookup(right, dp_ancestor->d_namep, 0);
	(void)db_get_external(&ext_ancestor, dp_ancestor, ancestor);
	if (dp_left != RT_DIR_NULL) (void)db_get_external(&ext_left, dp_left, left);
	if (dp_right != RT_DIR_NULL) (void)db_get_external(&ext_right, dp_right, right);

	/* (!dp_left && !dp_right) && dp_ancestor */
	if ((dp_left == RT_DIR_NULL) && (dp_right == RT_DIR_NULL)) {
	    if (del_func && del_func(left, ancestor, right, dp_left, dp_ancestor, dp_right, client_data)) error--;
	    has_diff++;
	}

	/* (dp_left && !dp_right) && (dp_ancestor == dp_left)  */
	if ((dp_left != RT_DIR_NULL) && (dp_right == RT_DIR_NULL) && !db_diff_external(&ext_ancestor, &ext_left)) {
	    if (del_func && del_func(left, ancestor, right, dp_left, dp_ancestor, dp_right, client_data)) error--;
	    has_diff++;
	}

	/* (dp_left && !dp_right) && (dp_ancestor != dp_left)  */
	if ((dp_left != RT_DIR_NULL) && (dp_right == RT_DIR_NULL) && db_diff_external(&ext_ancestor, &ext_left)) {
	    if (chgd_func && chgd_func(left, ancestor, DBI_NULL, dp_left, dp_ancestor, RT_DIR_NULL, client_data)) error--;
	    has_diff++;
	}

	/* (!dp_left && dp_right) && (dp_ancestor == dp_right) */
	if ((dp_left == RT_DIR_NULL) && (dp_right != RT_DIR_NULL) && !db_diff_external(&ext_ancestor, &ext_right)) {
	    if (del_func && del_func(left, ancestor, right, dp_left, dp_ancestor, dp_right, client_data)) error--;
	    has_diff++;
	}

	/* (!dp_left && dp_right) && (dp_ancestor != dp_right) */
	if ((dp_left == RT_DIR_NULL) && (dp_right != RT_DIR_NULL) && db_diff_external(&ext_ancestor, &ext_right)) {
	    if (chgd_func && chgd_func(DBI_NULL, ancestor, right, RT_DIR_NULL, dp_ancestor, dp_right, client_data)) error--;
	    has_diff++;
	}

	/* (dp_left == dp_right) && (dp_ancestor == dp_left)   */
	if (!db_diff_external(&ext_left, &ext_right) && !db_diff_external(&ext_ancestor, &ext_left)) {
	    if (unchgd_func && unchgd_func(left, ancestor, right, dp_left, dp_ancestor, dp_right, client_data)) error--;
	}

	/* (dp_left == dp_right) && (dp_ancestor != dp_left)   */
	if (!db_diff_external(&ext_left, &ext_right) && db_diff_external(&ext_ancestor, &ext_left)) {
	    if (chgd_func && chgd_func(left, ancestor, right, dp_left, dp_ancestor, dp_right, client_data)) error--;
	    has_diff++;
	}

	/* (dp_left != dp_right) && (dp_ancestor == dp_left)   */
	if (db_diff_external(&ext_left, &ext_right) && !db_diff_external(&ext_ancestor, &ext_left)) {
	    if (chgd_func && chgd_func(left, ancestor, right, dp_left, dp_ancestor, dp_right, client_data)) error--;
	    has_diff++;
	}

	/* (dp_left != dp_right) && (dp_ancestor == dp_right)  */
	if (db_diff_external(&ext_left, &ext_right) && !db_diff_external(&ext_ancestor, &ext_right)) {
	    if (chgd_func && chgd_func(left, ancestor, right, dp_left, dp_ancestor, dp_right, client_data)) error--;
	    has_diff++;
	}

	/* (dp_left != dp_right) && (dp_ancestor != dp_left && dp_ancestor != dp_right) */
	if (db_diff_external(&ext_left, &ext_right) && db_diff_external(&ext_ancestor, &ext_left) && db_diff_external(&ext_ancestor, &ext_right)) {
	    if (chgd_func && chgd_func(left, ancestor, right, dp_left, dp_ancestor, dp_right, client_data)) error--;
	    has_diff++;
	}
    } FOR_ALL_DIRECTORY_END;

    FOR_ALL_DIRECTORY_START(dp_left, left) {
	dp_ancestor = db_lookup(ancestor, dp_left->d_namep, 0);
	if (dp_ancestor == RT_DIR_NULL) {
	    struct bu_external ext_left, ext_right;
	    dp_right = db_lookup(right, dp_left->d_namep, 0);
	    (void)db_get_external(&ext_left, dp_left, left);
	    if (dp_right != RT_DIR_NULL) (void)db_get_external(&ext_right, dp_right, right);
	    /* dp_left && !dp_right || dp_left == dp_right */
	    if (dp_right == RT_DIR_NULL || !db_diff_external(&ext_left, &ext_right)) {
		if (add_func && add_func(left, ancestor, right, dp_left, dp_ancestor, dp_right, client_data)) error--;
		has_diff++;
	    }
	    /* dp_left != dp_right */
	    if (dp_right != RT_DIR_NULL && db_diff_external(&ext_left, &ext_right)) {
		if (chgd_func && chgd_func(left, ancestor, right, dp_left, dp_ancestor, dp_right, client_data)) error--;
		has_diff++;
	    }
	}
    } FOR_ALL_DIRECTORY_END;

    FOR_ALL_DIRECTORY_START(dp_right, right) {
	dp_ancestor = db_lookup(ancestor, dp_right->d_namep, 0);
	dp_left = db_lookup(left, dp_right->d_namep, 0);
	if (dp_ancestor == RT_DIR_NULL && dp_left == RT_DIR_NULL) {
	    if (add_func && add_func(left, ancestor, right, dp_left, dp_ancestor, dp_right, client_data)) error--;
	    has_diff++;
	}
    } FOR_ALL_DIRECTORY_END;

    return has_diff;
}


HIDDEN int
avpp_val_compare(const char *val1, const char *val2, const struct bn_tol *diff_tol)
{
    /* We need to look for numbers to do tolerance based comparisons */
    int num_compare = 1;
    int pnt_compare = 1;
    double dval1, dval2;
    float p1val1, p1val2, p1val3;
    float p2val1, p2val2, p2val3;
    char *endptr;

    /* First, check for individual numbers */
    errno = 0;
    dval1 = strtod(val1, &endptr);
    if (errno == EINVAL || *endptr != '\0') num_compare--;
    errno = 0;
    dval2 = strtod(val2, &endptr);
    if (errno == EINVAL || *endptr != '\0') num_compare--;

    /* If we didn't find numbers, try for points (3 floating point numbers) */
    if (num_compare != 1) {
	if (!sscanf(val1, "%f %f %f", &p1val1, &p1val2, &p1val3)) pnt_compare--;
	if (!sscanf(val2, "%f %f %f", &p2val1, &p2val2, &p2val3)) pnt_compare--;
    }

    if (num_compare == 1) {
	return NEAR_EQUAL(dval1, dval2, diff_tol->dist);
    }
    if (pnt_compare == 1) {
	vect_t v1, v2;
	VSET(v1, p1val1, p1val2, p1val3);
	VSET(v2, p2val1, p2val2, p2val3);
	return VNEAR_EQUAL(v1, v2, diff_tol->dist);
    }
    return BU_STR_EQUAL(val1, val2);
}

int
db_avs_diff(
	struct bu_attribute_value_set *added,
	struct bu_attribute_value_set *removed,
	struct bu_attribute_value_set *changed_left,
	struct bu_attribute_value_set *changed_right,
	struct bu_attribute_value_set *unchanged,
	const struct bu_attribute_value_set *left_set,
	const struct bu_attribute_value_set *right_set,
	const struct bn_tol *diff_tol)
{
    int state = DIFF_UNCHANGED;
    struct bu_attribute_value_pair *avp;
    if (unchanged && !BU_AVS_IS_INITIALIZED(unchanged)) BU_AVS_INIT(unchanged);
    if (removed && !BU_AVS_IS_INITIALIZED(removed)) BU_AVS_INIT(removed);
    if (added && !BU_AVS_IS_INITIALIZED(added)) BU_AVS_INIT(added);
    if (changed_left && !BU_AVS_IS_INITIALIZED(changed_left)) BU_AVS_INIT(changed_left);
    if (changed_right && !BU_AVS_IS_INITIALIZED(changed_right)) BU_AVS_INIT(changed_right);
    for (BU_AVS_FOR(avp, left_set)) {
	const char *val2 = bu_avs_get(right_set, avp->name);
	if (!val2) {
	    if (removed) {
		(void)bu_avs_add(removed, avp->name, avp->value);
	    }
	    state |= DIFF_REMOVED;
	} else {
	    if (avpp_val_compare(avp->value, val2, diff_tol)) {
		if (unchanged) {
		    (void)bu_avs_add(unchanged, avp->name, avp->value);
		}
	    } else {
		if (changed_left) {
		    (void)bu_avs_add(changed_left, avp->name, avp->value);
		}
		if (changed_right) {
		    (void)bu_avs_add(changed_right, avp->name, val2);
		}
		state |= DIFF_CHANGED;
	    }
	}
    }
    for (BU_AVS_FOR(avp, right_set)) {
	const char *val1 = bu_avs_get(left_set, avp->name);
	if (!val1) {
	    if (added) {
		(void)bu_avs_add(added, avp->name, avp->value);
	    }
	    state |= DIFF_ADDED;
	}
    }
    return state;
}


/*******************************************************************/
/* diff3 logic for attribute/value sets                            */
/*******************************************************************/
HIDDEN int
bu_avs_diff3(struct bu_attribute_value_set *unchanged,
	struct bu_attribute_value_set *removed_avs1_only,
	struct bu_attribute_value_set *removed_avs2_only,
	struct bu_attribute_value_set *removed_both,
	struct bu_attribute_value_set *added_avs1_only,
	struct bu_attribute_value_set *added_avs2_only,
	struct bu_attribute_value_set *added_both,
	struct bu_attribute_value_set *added_conflict_avs1,
	struct bu_attribute_value_set *added_conflict_avs2,
	struct bu_attribute_value_set *changed_avs1_only,
	struct bu_attribute_value_set *changed_avs2_only,
	struct bu_attribute_value_set *changed_both,
	struct bu_attribute_value_set *changed_conflict_ancestor,
	struct bu_attribute_value_set *changed_conflict_avs1,
	struct bu_attribute_value_set *changed_conflict_avs2,
	struct bu_attribute_value_set *merged,
	const struct bu_attribute_value_set *ancestor,
	const struct bu_attribute_value_set *avs1,
	const struct bu_attribute_value_set *avs2,
	const struct bn_tol *diff_tol)
{
    int have_diff = 0;
    struct bu_attribute_value_pair *avp;
    if (!BU_AVS_IS_INITIALIZED(unchanged)) BU_AVS_INIT(unchanged);
    if (!BU_AVS_IS_INITIALIZED(removed_avs1_only)) BU_AVS_INIT(removed_avs1_only);
    if (!BU_AVS_IS_INITIALIZED(removed_avs2_only)) BU_AVS_INIT(removed_avs2_only);
    if (!BU_AVS_IS_INITIALIZED(removed_both)) BU_AVS_INIT(removed_both);
    if (!BU_AVS_IS_INITIALIZED(added_avs1_only)) BU_AVS_INIT(added_avs1_only);
    if (!BU_AVS_IS_INITIALIZED(added_avs2_only)) BU_AVS_INIT(added_avs2_only);
    if (!BU_AVS_IS_INITIALIZED(added_both)) BU_AVS_INIT(added_both);
    if (!BU_AVS_IS_INITIALIZED(added_conflict_avs1)) BU_AVS_INIT(added_conflict_avs1);
    if (!BU_AVS_IS_INITIALIZED(added_conflict_avs2)) BU_AVS_INIT(added_conflict_avs2);
    if (!BU_AVS_IS_INITIALIZED(changed_avs1_only)) BU_AVS_INIT(changed_avs1_only);
    if (!BU_AVS_IS_INITIALIZED(changed_avs2_only)) BU_AVS_INIT(changed_avs2_only);
    if (!BU_AVS_IS_INITIALIZED(changed_both)) BU_AVS_INIT(changed_both);
    if (!BU_AVS_IS_INITIALIZED(changed_conflict_ancestor)) BU_AVS_INIT(changed_conflict_ancestor);
    if (!BU_AVS_IS_INITIALIZED(changed_conflict_avs1)) BU_AVS_INIT(changed_conflict_avs1);
    if (!BU_AVS_IS_INITIALIZED(changed_conflict_avs2)) BU_AVS_INIT(changed_conflict_avs2);
    if (!BU_AVS_IS_INITIALIZED(merged)) BU_AVS_INIT(merged);

    for (BU_AVS_FOR(avp, ancestor)) {
	const char *val_ancestor = bu_avs_get(ancestor, avp->name);
	const char *val1 = bu_avs_get(avs1, avp->name);
	const char *val2 = bu_avs_get(avs2, avp->name);
	/* The possibilities are:
	 *
	 * (!val1 && !val2) && val_ancestor
	 * (val1 && !val2) && (val_ancestor == val1)
	 * (val1 && !val2) && (val_ancestor != val1)
	 * (!val1 && val2) && (val_ancestor == val2)
	 * (!val1 && val2) && (val_ancestor != val2)
	 * (val1 == val2) && (val_ancestor == val1)
	 * (val1 == val2) && (val_ancestor != val1)
	 * (val1 != val2) && (val_ancestor == val1)
	 * (val1 != val2) && (val_ancestor == val2)
	 * (val1 != val2) && (val_ancestor != val1 && val_ancestor != val2)
	 */

	/* Removed from both - no conflict, nothing to merge */
	if ((!val1 && !val2) && val_ancestor) {
	    (void)bu_avs_add(removed_both, avp->name, val_ancestor);
	    have_diff++;
	}

	/* Removed from avs2 only, avs1 not changed - no conflict,
	 * avs2 removal wins and avs1 is not merged */
	if ((val1 && !val2) && avpp_val_compare(val_ancestor, val1, diff_tol)) {
	    (void)bu_avs_add(removed_avs2_only, avp->name, val_ancestor);
	    have_diff++;
	}

	/* Removed from avs2 only, avs1 changed - conflict
	 * merge adds conflict a/v pairs */
	if ((val1 && !val2) && !avpp_val_compare(val_ancestor, val1, diff_tol)) {
	    struct bu_vls avname = BU_VLS_INIT_ZERO;
	    struct bu_vls avval = BU_VLS_INIT_ZERO;
	    (void)bu_avs_add(changed_conflict_ancestor, avp->name, val_ancestor);
	    (void)bu_avs_add(changed_conflict_avs1, avp->name, val1);
	    bu_vls_sprintf(&avname, "CONFLICT(ANCESTOR):%s", avp->name);
	    (void)bu_avs_add(merged, bu_vls_addr(&avname), val_ancestor);
	    bu_vls_sprintf(&avname, "CONFLICT(left):%s", avp->name);
	    (void)bu_avs_add(merged, bu_vls_addr(&avname), val1);
	    bu_vls_sprintf(&avname, "CONFLICT(right):%s", avp->name);
	    bu_vls_sprintf(&avval, "%s", "REMOVED");
	    (void)bu_avs_add(merged, bu_vls_addr(&avname), bu_vls_addr(&avval));
	    bu_vls_free(&avname);
	    bu_vls_free(&avval);
	    have_diff++;
	}

	/* Removed from avs1 only, avs2 not changed - no conflict,
	 * avs1 change wins and avs2 not merged */
	if ((!val1 && val2) && avpp_val_compare(val_ancestor, val2, diff_tol)) {
	    (void)bu_avs_add(removed_avs1_only, avp->name, val_ancestor);
	    have_diff++;
	}

	/* Removed from avs1 only, avs2 changed - conflict,
	 * merge defaults to preserving information */
	if ((!val1 && val2) && !avpp_val_compare(val_ancestor, val2, diff_tol)) {
	    struct bu_vls avname = BU_VLS_INIT_ZERO;
	    struct bu_vls avval = BU_VLS_INIT_ZERO;
	    (void)bu_avs_add(changed_conflict_ancestor, avp->name, val_ancestor);
	    (void)bu_avs_add(changed_conflict_avs2, avp->name, val2);
	    bu_vls_sprintf(&avname, "CONFLICT(ANCESTOR):%s", avp->name);
	    (void)bu_avs_add(merged, bu_vls_addr(&avname), val_ancestor);
	    bu_vls_sprintf(&avname, "CONFLICT(left):%s", avp->name);
	    bu_vls_sprintf(&avval, "%s", "REMOVED");
	    (void)bu_avs_add(merged, bu_vls_addr(&avname), bu_vls_addr(&avval));
	    bu_vls_sprintf(&avname, "CONFLICT(right):%s", avp->name);
	    (void)bu_avs_add(merged, bu_vls_addr(&avname), val2);
	    bu_vls_free(&avname);
	    bu_vls_free(&avval);
	    have_diff++;
	}

	/* All values equal, unchanged and merged */
	if (avpp_val_compare(val1, val2, diff_tol) && avpp_val_compare(val_ancestor, val1, diff_tol)) {
	    (void)bu_avs_add(unchanged, avp->name, val_ancestor);
	    (void)bu_avs_add(merged, avp->name, val_ancestor);
	}
	/* Identical change to both - changed and merged */
	if (avpp_val_compare(val1, val2, diff_tol) && !avpp_val_compare(val_ancestor, val1, diff_tol)) {
	    (void)bu_avs_add(changed_both, avp->name, val1);
	    (void)bu_avs_add(merged, avp->name, val1);
	    have_diff++;
	}
	/* val2 changed, val1 not changed - val2 change wins and is merged */
	if (!avpp_val_compare(val1, val2, diff_tol) && avpp_val_compare(val_ancestor, val1, diff_tol)) {
	    (void)bu_avs_add(changed_avs2_only, avp->name, val2);
	    (void)bu_avs_add(merged, avp->name, val2);
	    have_diff++;
	}
	/* val1 changed, val2 not changed - val1 change wins and is merged */
	if (!avpp_val_compare(val1, val2, diff_tol) && avpp_val_compare(val_ancestor, val2, diff_tol)) {
	    (void)bu_avs_add(changed_avs1_only, avp->name, val1);
	    (void)bu_avs_add(merged, avp->name, val1);
	    have_diff++;
	}
	/* val1 and val2 changed and incompatible - conflict,
	 * merge adds conflict a/v pairs */
	if (!avpp_val_compare(val1, val2, diff_tol) && !avpp_val_compare(val_ancestor, val1, diff_tol) && !avpp_val_compare(val_ancestor, val2, diff_tol)) {
	    struct bu_vls avname = BU_VLS_INIT_ZERO;
	    (void)bu_avs_add(changed_conflict_ancestor, avp->name, val_ancestor);
	    (void)bu_avs_add(changed_conflict_avs1, avp->name, val1);
	    (void)bu_avs_add(changed_conflict_avs2, avp->name, val2);
	    bu_vls_sprintf(&avname, "CONFLICT(ANCESTOR):%s", avp->name);
	    (void)bu_avs_add(merged, bu_vls_addr(&avname), val_ancestor);
	    bu_vls_sprintf(&avname, "CONFLICT(LEFT):%s", avp->name);
	    (void)bu_avs_add(merged, bu_vls_addr(&avname), val1);
	    bu_vls_sprintf(&avname, "CONFLICT(RIGHT):%s", avp->name);
	    (void)bu_avs_add(merged, bu_vls_addr(&avname), val2);
	    bu_vls_free(&avname);
	    have_diff++;
	}
    }

    /* Now do avs1 - anything in ancestor has already been handled */
    for (BU_AVS_FOR(avp, avs1)) {
	const char *val_ancestor = bu_avs_get(ancestor, avp->name);
	if (!val_ancestor) {
	    const char *val1 = bu_avs_get(avs1, avp->name);
	    const char *val2 = bu_avs_get(avs2, avp->name);
	    int have_same_val;
	    have_diff++;
	    /* The possibilities are:
	     *
	     * (val1 && !val2)
	     * (val1 == val2)
	     * (val1 != val2)
	     */

	    /* Added in avs1 only - no conflict */
	    if (val1 && !val2) {
		(void)bu_avs_add(added_avs1_only, avp->name, val1);
		(void)bu_avs_add(merged, avp->name, val1);
	    }

	    have_same_val = avpp_val_compare(val1,val2, diff_tol);

	    /* Added in avs1 and avs2 with the same value - no conflict */
	    if (have_same_val) {
		(void)bu_avs_add(added_both, avp->name, val1);
		(void)bu_avs_add(merged, avp->name, val1);
	    } else {
		struct bu_vls avname = BU_VLS_INIT_ZERO;
		(void)bu_avs_add(added_conflict_avs1, avp->name, val1);
		(void)bu_avs_add(added_conflict_avs2, avp->name, val2);
		bu_vls_sprintf(&avname, "CONFLICT(LEFT):%s", avp->name);
		(void)bu_avs_add(merged, bu_vls_addr(&avname), val1);
		bu_vls_sprintf(&avname, "CONFLICT(RIGHT):%s", avp->name);
		(void)bu_avs_add(merged, bu_vls_addr(&avname), val2);
		bu_vls_free(&avname);
	    }
	}
    }

    /* Last but not least, avs2 - anything in ancestor and/or avs1 has already been handled */
    for (BU_AVS_FOR(avp, avs2)) {
	const char *val_ancestor = bu_avs_get(ancestor, avp->name);
	const char *val1 = bu_avs_get(avs1, avp->name);
	if (!val_ancestor && !val1) {
	    const char *val2 = bu_avs_get(avs2, avp->name);
	    (void)bu_avs_add(added_avs2_only, avp->name, val1);
	    (void)bu_avs_add(merged, avp->name, val2);
	    have_diff++;
	}
    }

    return (have_diff) ? 1 : 0;
}


HIDDEN int
tcl_list_to_avs(const char *tcl_list, struct bu_attribute_value_set *avs, int offset)
{
    int i = 0;
    int list_c = 0;
    const char **listv = (const char **)NULL;

    if (Tcl_SplitList(NULL, tcl_list, &list_c, (const char ***)&listv) != TCL_OK) {
	return -1;
    }

    if (!BU_AVS_IS_INITIALIZED(avs)) BU_AVS_INIT(avs);

    if (!list_c) {
	Tcl_Free((char *)listv);
	return 0;
    }

    for (i = offset; i < list_c; i += 2) {
	(void)bu_avs_add(avs, listv[i], listv[i+1]);
    }

    Tcl_Free((char *)listv);
    return 0;
}

/* TODO - this should be a function somewhere, is it already? */
HIDDEN const char *
arb_type_to_str(int type) {
    switch (type) {
	case 4:
	    return "arb4";
	    break;
	case 5:
	    return "arb5";
	    break;
	case 6:
	    return "arb6";
	    break;
	case 7:
	    return "arb7";
	    break;
	case 8:
	    return "arb8";
	    break;
	default:
	    return NULL;
	    break;
    }
    return NULL;
}

int
db_compare(const struct rt_db_internal *left_obj,
	const struct rt_db_internal *right_obj,
	int (*add_func)(const struct rt_db_internal *left_obj, const struct rt_db_internal *right_obj, int param_state, int attr_state, db_compare_criteria_t flags, void *data),
	int (*del_func)(const struct rt_db_internal *left_obj, const struct rt_db_internal *right_obj, int param_state, int attr_state, db_compare_criteria_t flags, void *data),
	int (*chgd_func)(const struct rt_db_internal *left_obj, const struct rt_db_internal *right_obj, int param_state, int attr_state, db_compare_criteria_t flags, void *data),
	int (*unchgd_func)(const struct rt_db_internal *left_obj, const struct rt_db_internal *right_obj, int param_state, int attr_state, db_compare_criteria_t flags, void *data),
	db_compare_criteria_t flags,
	void *client_data)
{
    int do_all = 0;
    int state = DIFF_UNCHANGED;
    int param_state = DIFF_UNCHANGED;
    int attr_state = DIFF_UNCHANGED;

    if (!left_obj && !right_obj) return -1;

    if (left_obj && !right_obj) {
	param_state |= DIFF_REMOVED;
	attr_state |= DIFF_REMOVED;
	state |= DIFF_REMOVED;
    }
    if (!left_obj && right_obj) {
	param_state |= DIFF_ADDED;
	attr_state |= DIFF_ADDED;
	state |= DIFF_ADDED;
    }

    if (flags == DB_COMPARE_ALL) do_all = 1;

    if ((flags == DB_COMPARE_PARAM || do_all) && (left_obj && right_obj) && (param_state == DIFF_UNCHANGED)) {
	/* Type is a valid basis on which to declare a DB_COMPARE_PARAM difference event,
	 * but as a single value in the rt_<type>_get return it does not fit neatly into
	 * the attribute/value paradigm used for the majority of the comparisons.  For
	 * this reason, we handle it specially using the lower level database type
	 * information directly.
	 */
	if (left_obj->idb_minor_type != right_obj->idb_minor_type) {
	    param_state |= DIFF_CHANGED;
	} else {
	    if (left_obj->idb_minor_type == DB5_MINORTYPE_BRLCAD_ARB8) {
		struct bn_tol arb_tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1.0 - 1e-6 };
		int arb_type_1 = rt_arb_std_type(left_obj, &arb_tol);
		int arb_type_2 = rt_arb_std_type(right_obj, &arb_tol);
		if (arb_type_1 != arb_type_2) {
		    param_state |= DIFF_CHANGED;
		}
	    }
	}

	/* Compare the idb_ptr memory, if we aren't already sure we've changed. */
	if (!param_state) {
	    int memsize = rt_intern_struct_size(left_obj->idb_minor_type);
	    if (memcmp((void *)left_obj->idb_ptr, (void *)right_obj->idb_ptr, memsize)) {
		param_state |= DIFF_CHANGED;
	    }
	}
    }

    if ((flags == DB_COMPARE_ATTRS || do_all) && (left_obj && right_obj) && (param_state == DIFF_UNCHANGED)) {
	if (left_obj->idb_avs.magic == BU_AVS_MAGIC && right_obj->idb_avs.magic == BU_AVS_MAGIC) {
	    struct bn_tol diff_tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1.0 - 1e-6 };
	    attr_state |= db_avs_diff(NULL, NULL, NULL, NULL, NULL, &left_obj->idb_avs, &right_obj->idb_avs, &diff_tol);
	} else {
	    if (left_obj->idb_avs.magic == BU_AVS_MAGIC) {attr_state |= DIFF_REMOVED;}
	    if (right_obj->idb_avs.magic == BU_AVS_MAGIC) {attr_state |= DIFF_ADDED;}
	}
    }

    state |= param_state;
    state |= attr_state;

    /* We know enough now to identify the state of the diff - if we also have user functions, call them now */
    if ((state & DIFF_ADDED) && add_func) add_func(left_obj, right_obj, param_state, attr_state, flags, client_data);
    if ((state & DIFF_REMOVED) && del_func) del_func(left_obj, right_obj, param_state, attr_state, flags, client_data);
    if ((state & DIFF_CHANGED) && chgd_func) chgd_func(left_obj, right_obj, param_state, attr_state, flags, client_data);
    if ((state == DIFF_UNCHANGED) && unchgd_func) unchgd_func(left_obj, right_obj, param_state, attr_state, flags, client_data);
    return state;
}

#if 0
HIDDEN const char *
type_to_str(const struct rt_db_internal *obj, int arb_type) {
    if (arb_type) return arb_type_to_str(arb_type);
    return obj->idb_meth->ft_label;
}
#endif

int
db_compare3(const struct rt_db_internal *left,
	    const struct rt_db_internal *ancestor,
	    const struct rt_db_internal *right,
	    int (*add_func)(const struct rt_db_internal *left_obj, const struct rt_db_internal *ancestor_obj, const struct rt_db_internal *right_obj, int param_state, int attr_state, db_compare_criteria_t flags, void *data),
	    int (*del_func)(const struct rt_db_internal *left_obj, const struct rt_db_internal *ancestor_obj, const struct rt_db_internal *right_obj, int param_state, int attr_state, db_compare_criteria_t flags, void *data),
	    int (*chgd_func)(const struct rt_db_internal *left_obj, const struct rt_db_internal *ancestor_obj, const struct rt_db_internal *right_obj, int param_state, int attr_state, db_compare_criteria_t flags, void *data),
	    int (*unchgd_func)(const struct rt_db_internal *left_obj, const struct rt_db_internal *ancestor_obj, const struct rt_db_internal *right_obj, int param_state, int attr_state, db_compare_criteria_t flags, void *data),
	    db_compare_criteria_t flags,
	    void *client_data)
{
    int do_all = 0;
    int state = DIFF3_UNCHANGED;
    int param_state = DIFF3_UNCHANGED;
    int attr_state = DIFF3_UNCHANGED;

    if (!left && !ancestor && !right) return -1;
    if (ancestor && !left && !right) {
	param_state |= DIFF3_REMOVED_BOTH_IDENTICALLY;
	attr_state |= DIFF3_REMOVED_BOTH_IDENTICALLY;
	state |= DIFF3_REMOVED_BOTH_IDENTICALLY;
    }

    if (flags == DB_COMPARE_ALL) do_all = 1;

    if ((flags == DB_COMPARE_PARAM || do_all) && (param_state == DIFF_UNCHANGED)) {
	int ancestor_left = -1;
	int ancestor_right = -1;
	int left_right = -1;
	/* Type is a valid basis on which to declare a DB_COMPARE_PARAM difference event,
	 * but as a single value in the rt_<type>_get return it does not fit neatly into
	 * the attribute/value paradigm used for the majority of the comparisons.  For
	 * this reason, we handle it specially using the lower level database type
	 * information directly.
	 */
	int a = -1;
	int l = -1;
	int r = -1;
	int a_arb = 0;
	int l_arb = 0;
	int r_arb = 0;
	if (ancestor) a = ancestor->idb_minor_type;
	if (right) r = right->idb_minor_type;
	if (left) l = left->idb_minor_type;
	if (a == DB5_MINORTYPE_BRLCAD_ARB8 || l == DB5_MINORTYPE_BRLCAD_ARB8 || r == DB5_MINORTYPE_BRLCAD_ARB8) {
	    struct bn_tol arb_tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1.0 - 1e-6 };
	    if (a == DB5_MINORTYPE_BRLCAD_ARB8) {a_arb = rt_arb_std_type(ancestor, &arb_tol);}
	    if (l == DB5_MINORTYPE_BRLCAD_ARB8) {l_arb = rt_arb_std_type(left, &arb_tol);}
	    if (r == DB5_MINORTYPE_BRLCAD_ARB8) {r_arb = rt_arb_std_type(right, &arb_tol);}
	}

	if (ancestor && left) {
	    if (a != l || a_arb != l_arb) ancestor_left = 0;
	    if (ancestor_left == -1) {
		int memsize = rt_intern_struct_size(ancestor->idb_minor_type);
		if (memcmp((void *)ancestor->idb_ptr, (void *)left->idb_ptr, memsize)) {
		    ancestor_left = 0;
		} else {
		    ancestor_left = 1;
		}
	    }
	}

	if (ancestor && right) {
	    if (a != r || a_arb != r_arb) ancestor_right = 0;
	    if (ancestor_right == -1) {
		int memsize = rt_intern_struct_size(ancestor->idb_minor_type);
		if (memcmp((void *)ancestor->idb_ptr, (void *)right->idb_ptr, memsize)) {
		    ancestor_right = 0;
		} else {
		    ancestor_right = 1;
		}
	    }
	}

	if (left && right) {
	    if (l != r || l_arb != r_arb) left_right = 0;
	    if (left_right == -1) {
		int memsize = rt_intern_struct_size(left->idb_minor_type);
		if (memcmp((void *)left->idb_ptr, (void *)right->idb_ptr, memsize)) {
		    left_right = 0;
		} else {
		    left_right = 1;
		}
	    }
	}

	if (ancestor_right && !left) param_state |= DIFF3_REMOVED_LEFT_ONLY;
	if (ancestor_left && !right) param_state |= DIFF3_REMOVED_RIGHT_ONLY;
	if (!ancestor && left_right) param_state |= DIFF3_ADDED_BOTH_IDENTICALLY;
	if (!ancestor && !left && right) param_state |= DIFF3_ADDED_RIGHT_ONLY;
	if (!ancestor && left && !right) param_state |= DIFF3_ADDED_LEFT_ONLY;
	if (!ancestor_left && !ancestor_right && left_right) param_state |= DIFF3_CHANGED_BOTH_IDENTICALLY;
	if (!ancestor_left && ancestor_right) param_state |= DIFF3_CHANGED_LEFT_ONLY;
	if (ancestor_left && !ancestor_right) param_state |= DIFF3_CHANGED_RIGHT_ONLY;
	if ((!ancestor || (!ancestor_left && !ancestor_right)) && !left_right) param_state |= DIFF3_CONFLICT_BOTH_CHANGED;
	if (ancestor && left && !ancestor_left && !right) param_state |= DIFF3_CONFLICT_LEFT_CHANGE_RIGHT_DEL;
	if (ancestor && right && !ancestor_right && !left) param_state |= DIFF3_CONFLICT_RIGHT_CHANGE_LEFT_DEL;

    }

    if ((flags == DB_COMPARE_ATTRS || do_all) && !attr_state) {
	struct bn_tol diff_tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1.0 - 1e-6 };
	const struct bu_attribute_value_set *a;
	const struct bu_attribute_value_set *l;
	const struct bu_attribute_value_set *r;
	struct bu_attribute_value_set at;
	struct bu_attribute_value_set lt;
	struct bu_attribute_value_set rt;
	BU_AVS_INIT(&at);
	BU_AVS_INIT(&lt);
	BU_AVS_INIT(&rt);

	if (ancestor && ancestor->idb_avs.magic == BU_AVS_MAGIC) {
	    a = &(ancestor->idb_avs);
	} else {
	    a = &at;
	}
	if (left->idb_avs.magic == BU_AVS_MAGIC) {
	    l = &(left->idb_avs);
	} else {
	    l = &lt;
	}
	if (right->idb_avs.magic == BU_AVS_MAGIC) {
	    r = &(right->idb_avs);
	} else {
	    r = &rt;
	}

	attr_state |= bu_avs_diff3(NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, a, l, r, &diff_tol);

	bu_avs_free(&at);
	bu_avs_free(&lt);
	bu_avs_free(&rt);

    }

    state |= param_state;
    state |= attr_state;

    /* We know enough now to identify the state of the diff - if we also have user functions, call them now */
    if (((state & DIFF3_ADDED_BOTH_IDENTICALLY) || (state & DIFF3_ADDED_LEFT_ONLY) || (state & DIFF3_ADDED_RIGHT_ONLY)) && add_func)
	add_func(left, ancestor, right, param_state, attr_state, flags, client_data);
    if (((state & DIFF3_REMOVED_BOTH_IDENTICALLY) || (state & DIFF3_REMOVED_LEFT_ONLY) || (state & DIFF3_REMOVED_RIGHT_ONLY)) && del_func)
       	del_func(left, ancestor, right, param_state, attr_state, flags, client_data);
    if (((state & DIFF3_CHANGED_BOTH_IDENTICALLY) || (state & DIFF3_CHANGED_LEFT_ONLY) || (state & DIFF3_CHANGED_RIGHT_ONLY) ||
		(state & DIFF3_CHANGED_CLEAN_MERGE)|| (state & DIFF3_CONFLICT_LEFT_CHANGE_RIGHT_DEL) ||
		(state & DIFF3_CONFLICT_RIGHT_CHANGE_LEFT_DEL) || (state & DIFF3_CONFLICT_BOTH_CHANGED)) && chgd_func)
       	chgd_func(left, ancestor, right, param_state, attr_state, flags, client_data);
    if ((state == DIFF3_UNCHANGED) && unchgd_func)
       	unchgd_func(left, ancestor, right, param_state, attr_state, flags, client_data);

    return state;

}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
