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

	/* skip the _GLOBAL object for now - need to deal with this, however */
	if (dp1->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	    continue;
	}

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

	/* skip the _GLOBAL object for now - need to deal with this, however */
	if (dp2->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	    continue;
	}

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

	/* skip the _GLOBAL object for now - need to deal with this, however */
	if (dp_ancestor->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	    continue;
	}
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

HIDDEN int
bu_avs_diff(struct bu_attribute_value_set *shared,
	struct bu_attribute_value_set *orig_only,
	struct bu_attribute_value_set *new_only,
	struct bu_attribute_value_set *orig_diff,
	struct bu_attribute_value_set *new_diff,
	const struct bu_attribute_value_set *avs1,
	const struct bu_attribute_value_set *avs2,
	const struct bn_tol *diff_tol)
{
    int have_diff = 0;
    struct bu_attribute_value_pair *avp;
    if (shared && !BU_AVS_IS_INITIALIZED(shared)) BU_AVS_INIT(shared);
    if (orig_only && !BU_AVS_IS_INITIALIZED(orig_only)) BU_AVS_INIT(orig_only);
    if (new_only && !BU_AVS_IS_INITIALIZED(new_only)) BU_AVS_INIT(new_only);
    if (orig_diff && !BU_AVS_IS_INITIALIZED(orig_diff)) BU_AVS_INIT(orig_diff);
    if (new_diff && !BU_AVS_IS_INITIALIZED(new_diff)) BU_AVS_INIT(new_diff);
    for (BU_AVS_FOR(avp, avs1)) {
	const char *val2 = bu_avs_get(avs2, avp->name);
	if (!val2) {
	    if (orig_only) {
		(void)bu_avs_add(orig_only, avp->name, avp->value);
	    }
	    have_diff++;
	} else {
	    if (avpp_val_compare(avp->value, val2, diff_tol)) {
		if (shared) {
		    (void)bu_avs_add(shared, avp->name, avp->value);
		}
	    } else {
		if (orig_diff) {
		    (void)bu_avs_add(orig_diff, avp->name, avp->value);
		}
		if (new_diff) {
		    (void)bu_avs_add(new_diff, avp->name, val2);
		}
		have_diff++;
	    }
	}
    }
    for (BU_AVS_FOR(avp, avs2)) {
	const char *val1 = bu_avs_get(avs1, avp->name);
	if (!val1) {
	    if (new_only) {
		(void)bu_avs_add(new_only, avp->name, avp->value);
	    }
	    have_diff++;
	}
    }
    return (have_diff) ? 1 : 0;
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
		have_diff++;
	    }
	    /* Added in avs1 and avs2 with the same value - no conflict */
	    if (avpp_val_compare(val1,val2, diff_tol)) {
	       	(void)bu_avs_add(added_both, avp->name, val1);
		(void)bu_avs_add(merged, avp->name, val1);
		have_diff++;
	    }
	    /* Added in avs1 and avs2 with different values - conflict
	     * merge adds conflict a/v pairs */
	    if (avpp_val_compare(val1,val2, diff_tol)) {
		struct bu_vls avname = BU_VLS_INIT_ZERO;
		(void)bu_avs_add(added_conflict_avs1, avp->name, val1);
		(void)bu_avs_add(added_conflict_avs2, avp->name, val2);
		bu_vls_sprintf(&avname, "CONFLICT(LEFT):%s", avp->name);
		(void)bu_avs_add(merged, bu_vls_addr(&avname), val1);
		bu_vls_sprintf(&avname, "CONFLICT(RIGHT):%s", avp->name);
		(void)bu_avs_add(merged, bu_vls_addr(&avname), val2);
		bu_vls_free(&avname);
		have_diff++;
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
	db_compare_criteria_t flags,
	struct bu_attribute_value_set *added,
	struct bu_attribute_value_set *removed,
	struct bu_attribute_value_set *changed_left,
	struct bu_attribute_value_set *changed_right,
	struct bu_attribute_value_set *unchanged,
	struct bn_tol *diff_tol)
{
    int do_all = 0;
    int has_diff = 0;
    int type_change = 0;

    if (!left_obj || !right_obj) return -1;
    if (added && !BU_AVS_IS_INITIALIZED(added)) BU_AVS_INIT(added);
    if (removed && !BU_AVS_IS_INITIALIZED(removed)) BU_AVS_INIT(removed);
    if (changed_left && !BU_AVS_IS_INITIALIZED(changed_left)) BU_AVS_INIT(changed_left);
    if (changed_right && !BU_AVS_IS_INITIALIZED(changed_right)) BU_AVS_INIT(changed_right);
    if (unchanged && !BU_AVS_IS_INITIALIZED(unchanged)) BU_AVS_INIT(unchanged);

    if (flags == DB_COMPARE_ALL) do_all = 1;

    if (flags == DB_COMPARE_PARAM || do_all) {
	int have_tcl1 = 1;
	int have_tcl2 = 1;
	struct bu_vls s1_tcl = BU_VLS_INIT_ZERO;
	struct bu_vls s2_tcl = BU_VLS_INIT_ZERO;
	struct bu_attribute_value_set avs1, avs2;
	BU_AVS_INIT(&avs1);
	BU_AVS_INIT(&avs2);

	/* Type is a valid basis on which to declare a DB_COMPARE_PARAM difference event,
	 * but as a single value in the rt_<type>_get return it does not fit neatly into
	 * the attribute/value paradigm used for the majority of the comparisons.  For
	 * this reason, we handle it specially using the lower level database type
	 * information directly.
	 */
	if (left_obj->idb_minor_type != right_obj->idb_minor_type) {
	    (void)bu_avs_add(changed_left, "object type", left_obj->idb_meth->ft_label);
	    (void)bu_avs_add(changed_right, "object type", right_obj->idb_meth->ft_label);
	    type_change = 1;
	} else {
	    if (left_obj->idb_minor_type == DB5_MINORTYPE_BRLCAD_ARB8) {
		struct bn_tol arb_tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1.0 - 1e-6 };
		int arb_type_1 = rt_arb_std_type(left_obj, &arb_tol);
		int arb_type_2 = rt_arb_std_type(right_obj, &arb_tol);
		if (arb_type_1 != arb_type_2) {
		    (void)bu_avs_add(changed_left, "object type", arb_type_to_str(arb_type_1));
		    (void)bu_avs_add(changed_right, "object type", arb_type_to_str(arb_type_2));
		    type_change = 1;
		}
	    }
	}

	/* Try to create attribute/value set definitions for these
	 * objects from their Tcl list definitions.  We use an
	 * offset of one because for all objects the first entry
	 * in the list is the type of the object, which we have
	 * just handled above */
	bu_vls_trunc(&s1_tcl, 0);
	if (left_obj->idb_meth->ft_get(&s1_tcl, left_obj, NULL) == BRLCAD_ERROR) have_tcl1 = 0;
	bu_vls_trunc(&s2_tcl, 0);
	if (right_obj->idb_meth->ft_get(&s2_tcl, right_obj, NULL) == BRLCAD_ERROR) have_tcl2 = 0;

	/* If the tcl conversions didn't succeed, we've reached the limit of what
	 * we can do with internal parameter diffing. Otherwise, diff the resulting
	 * a/v sets.*/
	if (have_tcl1 && have_tcl2) {
	    if (tcl_list_to_avs(bu_vls_addr(&s1_tcl), &avs1, 1)) have_tcl1 = 0;
	    if (tcl_list_to_avs(bu_vls_addr(&s2_tcl), &avs2, 1)) have_tcl2 = 0;
	}
	if (have_tcl1 && have_tcl2) {
	    has_diff += bu_avs_diff(unchanged, removed, added, changed_left, changed_right, &avs1, &avs2, diff_tol);
	} else {
	    if (!type_change) {
		/* We don't have the tcl list version of the internal parameters, so all
		 * we can do is compare the idb_ptr memory.*/
		int memsize = rt_intern_struct_size(left_obj->idb_minor_type);
		if (memcmp((void *)left_obj->idb_ptr, (void *)right_obj->idb_ptr, memsize)) {
		    has_diff = 1;
		}
	    }
	}
	bu_avs_free(&avs1);
	bu_avs_free(&avs2);

    }

    if (flags == DB_COMPARE_ATTRS || do_all) {

	if (left_obj->idb_avs.magic == BU_AVS_MAGIC && right_obj->idb_avs.magic == BU_AVS_MAGIC) {
	    has_diff += bu_avs_diff(unchanged, removed, added, changed_left, changed_right, &left_obj->idb_avs, &right_obj->idb_avs, diff_tol);
	} else {
	    if (left_obj->idb_avs.magic == BU_AVS_MAGIC) {
		if (removed) {
		    bu_avs_merge(removed, &left_obj->idb_avs);
		}
		has_diff++;
	    }
	    if (right_obj->idb_avs.magic == BU_AVS_MAGIC) {
		if (added) {
		    bu_avs_merge(added, &right_obj->idb_avs);
		}
		has_diff++;
	    }
	}

    }

    if (type_change) return 1;
    return (has_diff) ? 1 : 0;
}

HIDDEN const char *
type_to_str(const struct rt_db_internal *obj, int arb_type) {
    if (arb_type) return arb_type_to_str(arb_type);
    return obj->idb_meth->ft_label;
}

int
db_compare3(const struct rt_db_internal *left,
	const struct rt_db_internal *ancestor,
	const struct rt_db_internal *right,
	db_compare_criteria_t flags,
	struct bu_attribute_value_set *unchanged,
	struct bu_attribute_value_set *removed_left_only,
	struct bu_attribute_value_set *removed_right_only,
	struct bu_attribute_value_set *removed_both,
	struct bu_attribute_value_set *added_left_only,
	struct bu_attribute_value_set *added_right_only,
	struct bu_attribute_value_set *added_both,
	struct bu_attribute_value_set *added_conflict_left,
	struct bu_attribute_value_set *added_conflict_right,
	struct bu_attribute_value_set *changed_left_only,
	struct bu_attribute_value_set *changed_right_only,
	struct bu_attribute_value_set *changed_both,
	struct bu_attribute_value_set *changed_conflict_ancestor,
	struct bu_attribute_value_set *changed_conflict_left,
	struct bu_attribute_value_set *changed_conflict_right,
	struct bu_attribute_value_set *merged,
	struct bn_tol *diff_tol)
{
    int do_all = 0;
    int has_diff = 0;
    int type_change = 1;

    if (!left || !right) return -1;
    if (!BU_AVS_IS_INITIALIZED(unchanged)) BU_AVS_INIT(unchanged);
    if (!BU_AVS_IS_INITIALIZED(removed_left_only)) BU_AVS_INIT(removed_left_only);
    if (!BU_AVS_IS_INITIALIZED(removed_right_only)) BU_AVS_INIT(removed_right_only);
    if (!BU_AVS_IS_INITIALIZED(removed_both)) BU_AVS_INIT(removed_both);
    if (!BU_AVS_IS_INITIALIZED(added_left_only)) BU_AVS_INIT(added_left_only);
    if (!BU_AVS_IS_INITIALIZED(added_right_only)) BU_AVS_INIT(added_right_only);
    if (!BU_AVS_IS_INITIALIZED(added_both)) BU_AVS_INIT(added_both);
    if (!BU_AVS_IS_INITIALIZED(added_conflict_left)) BU_AVS_INIT(added_conflict_left);
    if (!BU_AVS_IS_INITIALIZED(added_conflict_right)) BU_AVS_INIT(added_conflict_right);
    if (!BU_AVS_IS_INITIALIZED(changed_left_only)) BU_AVS_INIT(changed_left_only);
    if (!BU_AVS_IS_INITIALIZED(changed_right_only)) BU_AVS_INIT(changed_right_only);
    if (!BU_AVS_IS_INITIALIZED(changed_both)) BU_AVS_INIT(changed_both);
    if (!BU_AVS_IS_INITIALIZED(changed_conflict_ancestor)) BU_AVS_INIT(changed_conflict_ancestor);
    if (!BU_AVS_IS_INITIALIZED(changed_conflict_left)) BU_AVS_INIT(changed_conflict_left);
    if (!BU_AVS_IS_INITIALIZED(changed_conflict_right)) BU_AVS_INIT(changed_conflict_right);
    if (!BU_AVS_IS_INITIALIZED(merged)) BU_AVS_INIT(merged);


    if (flags == DB_COMPARE_ALL) do_all = 1;

    if (flags == DB_COMPARE_PARAM || do_all) {
	int have_tcl_ancestor = 1;
	int have_tcl_left = 1;
	int have_tcl_right = 1;
	struct bu_vls ancestor_tcl = BU_VLS_INIT_ZERO;
	struct bu_vls left_tcl = BU_VLS_INIT_ZERO;
	struct bu_vls right_tcl = BU_VLS_INIT_ZERO;
	struct bu_attribute_value_set ancestor_avs, left_avs, right_avs;
	BU_AVS_INIT(&ancestor_avs);
	BU_AVS_INIT(&left_avs);
	BU_AVS_INIT(&right_avs);

	/* Type is a valid basis on which to declare a DB_COMPARE_PARAM difference event,
	 * but as a single value in the rt_<type>_get return it does not fit neatly into
	 * the attribute/value paradigm used for the majority of the comparisons.  For
	 * this reason, we handle it specially using the lower level database type
	 * information directly.
	 *
	 * Cases:
	 *
	 * ancestor == left && left == right
	 * ancestor != left && left == right
	 * ancestor != left && ancestor == right && left != right
	 * ancestor == left && ancestor != right && left != right
	 * ancestor != left && ancestor != right && left != right
	 */
	{
	    int a = ancestor->idb_minor_type;   
	    int l = left->idb_minor_type;   
	    int r = right->idb_minor_type; 
	    int a_arb = 0;
	    int l_arb = 0;
	    int r_arb = 0;
	    if (a == DB5_MINORTYPE_BRLCAD_ARB8 || l == DB5_MINORTYPE_BRLCAD_ARB8 || r == DB5_MINORTYPE_BRLCAD_ARB8) {
		struct bn_tol arb_tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1.0 - 1e-6 };
		if (a == DB5_MINORTYPE_BRLCAD_ARB8) {a_arb = rt_arb_std_type(ancestor, &arb_tol);}
		if (l == DB5_MINORTYPE_BRLCAD_ARB8) {l_arb = rt_arb_std_type(left, &arb_tol);}
		if (r == DB5_MINORTYPE_BRLCAD_ARB8) {r_arb = rt_arb_std_type(right, &arb_tol);}
	    }

	    /* ancestor == left && left == right */
	    if ( (a == l) && (l == r) ) {
		(void)bu_avs_add(unchanged, "object type", type_to_str(ancestor, a_arb));
		(void)bu_avs_add(merged, "object type", type_to_str(ancestor, a_arb));
		type_change = 0;
	    }
	    /* ancestor != left && left == right */
	    if ( (a != l) && (l == r) ) {
		(void)bu_avs_add(changed_both, "object type", type_to_str(left, l_arb));
		(void)bu_avs_add(merged, "object type", type_to_str(left, l_arb));
	    }
	    /* ancestor != left && ancestor == right && left != right */
	    if ( (a != l) && (a == r) && (l != r) ) {
		(void)bu_avs_add(changed_left_only, "object type", type_to_str(left, l_arb));
		(void)bu_avs_add(merged, "object type", type_to_str(left, l_arb));
	    }
	    /* ancestor == left && ancestor != right && left != right */
	    if ( (a == l) && (a != r) && (l != r) ) {
		(void)bu_avs_add(changed_right_only, "object type", type_to_str(right, r_arb));
		(void)bu_avs_add(merged, "object type", type_to_str(right, r_arb));
	    }
	    /* ancestor != left && ancestor != right && left != right */
	    if ( (a != l) && (a != r) && (l != r) ) {
		(void)bu_avs_add(changed_conflict_ancestor, "object type", type_to_str(ancestor, a_arb));
		(void)bu_avs_add(changed_conflict_left, "object type", type_to_str(left, l_arb));
		(void)bu_avs_add(changed_conflict_right, "object type", type_to_str(right, r_arb));
		(void)bu_avs_add(merged, "CONFLICT(ANCESTOR):object type", type_to_str(ancestor, a_arb));
		(void)bu_avs_add(merged, "CONFLICT(LEFT):object type", type_to_str(left, l_arb));
		(void)bu_avs_add(merged, "CONFLICT(RIGHT):object type", type_to_str(right, r_arb));
	    }
	}

	/* Try to create attribute/value set definitions for these
	 * objects from their Tcl list definitions.  We use an
	 * offset of one because for all objects the first entry
	 * in the list is the type of the object, which we have
	 * just handled above */
	bu_vls_trunc(&ancestor_tcl, 0);
	if (ancestor->idb_meth->ft_get(&ancestor_tcl, ancestor, NULL) == BRLCAD_ERROR) have_tcl_ancestor = 0;
	bu_vls_trunc(&left_tcl, 0);
	if (left->idb_meth->ft_get(&left_tcl, left, NULL) == BRLCAD_ERROR) have_tcl_left = 0;
	bu_vls_trunc(&right_tcl, 0);
	if (right->idb_meth->ft_get(&right_tcl, right, NULL) == BRLCAD_ERROR) have_tcl_right = 0;

	/* If the tcl conversions didn't succeed, we've reached the limit of what
	 * we can do with internal parameter diffing. Otherwise, diff the resulting
	 * a/v sets.*/
	if (have_tcl_ancestor && have_tcl_left && have_tcl_right) {
	    if (tcl_list_to_avs(bu_vls_addr(&ancestor_tcl), &ancestor_avs, 1)) have_tcl_ancestor = 0;
	    if (tcl_list_to_avs(bu_vls_addr(&left_tcl), &left_avs, 1)) have_tcl_left = 0;
	    if (tcl_list_to_avs(bu_vls_addr(&right_tcl), &right_avs, 1)) have_tcl_right = 0;
	}
	if (have_tcl_ancestor && have_tcl_left && have_tcl_right) {
	    has_diff += bu_avs_diff3(unchanged, removed_left_only, removed_right_only, removed_both,
		    added_left_only, added_right_only, added_both, added_conflict_left, added_conflict_right,
		    changed_left_only, changed_right_only, changed_both, changed_conflict_ancestor,
		    changed_conflict_left, changed_conflict_right, merged, &ancestor_avs, &left_avs, &right_avs, diff_tol);
	} else {
	    if (!type_change) {
		/* We don't have the tcl list version of the internal parameters, so all
		 * we can do is compare the idb_ptr memory.  Cases:
		 *
		 * ancestor == left && left == right
		 * ancestor != left && left == right
		 * ancestor != left && ancestor == right && left != right
		 * ancestor == left && ancestor != right && left != right
		 * ancestor != left && ancestor != right && left != right
		 * */
		void *a = (void *)ancestor->idb_ptr;
		void *l = (void *)left->idb_ptr;
		void *r = (void *)right->idb_ptr;
		int memsize = rt_intern_struct_size(ancestor->idb_minor_type);

		/* ancestor == left && left == right */
		if (!memcmp(a, l, memsize) && !memcmp(l, r, memsize)) {
		    (void)bu_avs_add(unchanged, "PARAMS", "NO_CHANGE");
		    (void)bu_avs_add(merged, "PARAMS", "NO_CHANGE");
		}
		/* ancestor != left && left == right */
		if (memcmp(a, l, memsize) && !memcmp(l, r, memsize)) {
		    (void)bu_avs_add(changed_both, "PARAMS", "BOTH_BINARY");
		    (void)bu_avs_add(merged, "PARAMS", "BOTH_BINARY");
		    has_diff = 1;
		}
	    /* ancestor != left && ancestor == right && left != right */
		if (memcmp(a, l, memsize) && !memcmp(a, r, memsize) && memcmp(l, r, memsize)) {
		    (void)bu_avs_add(changed_left_only, "PARAMS", "LEFT_BINARY");
		    (void)bu_avs_add(merged, "PARAMS", "LEFT_BINARY");
		    has_diff = 1;
		}
	    /* ancestor == left && ancestor != right && left != right */
		if (!memcmp(a, l, memsize) && memcmp(a, r, memsize) && memcmp(l, r, memsize)) {
		    (void)bu_avs_add(changed_right_only, "PARAMS", "RIGHT_BINARY");
		    (void)bu_avs_add(merged, "PARAMS", "RIGHT_BINARY");
		    has_diff = 1;
		}
	    /* ancestor != left && ancestor != right && left != right */
		if (memcmp(a, l, memsize) && memcmp(a, r, memsize) && memcmp(l, r, memsize)) {
		    (void)bu_avs_add(changed_conflict_ancestor, "PARAMS", "BINARY_CONFLICT");
		    (void)bu_avs_add(changed_conflict_left, "PARAMS", "BINARY_CONFLICT");
		    (void)bu_avs_add(changed_conflict_right, "PARAMS", "BINARY_CONFLICT");
		    (void)bu_avs_add(merged, "PARAMS", "BINARY_CONFLICT");
		    has_diff = 1;
		}
	    }
	}
    }

    if (flags == DB_COMPARE_ATTRS || do_all) {
	const struct bu_attribute_value_set *a;
	const struct bu_attribute_value_set *l;
	const struct bu_attribute_value_set *r;
	struct bu_attribute_value_set at;
	struct bu_attribute_value_set lt;
	struct bu_attribute_value_set rt;
	BU_AVS_INIT(&at);
	BU_AVS_INIT(&lt);
	BU_AVS_INIT(&rt);

	if (ancestor->idb_avs.magic == BU_AVS_MAGIC) {
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

	has_diff += bu_avs_diff3(unchanged, removed_left_only, removed_right_only, removed_both,
		added_left_only, added_right_only, added_both, added_conflict_left, added_conflict_right,
		changed_left_only, changed_right_only, changed_both, changed_conflict_ancestor,
		changed_conflict_left, changed_conflict_right, merged, a, l, r, diff_tol);

	bu_avs_free(&at);
	bu_avs_free(&lt);
	bu_avs_free(&rt);

    }

    if (type_change) return 1;
    return (has_diff) ? 1 : 0;

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
