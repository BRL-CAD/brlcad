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

    if (list_c > 2) {
	for (i = offset; i < list_c; i += 2) {
	    (void)bu_avs_add(avs, listv[i], listv[i+1]);
	}
    } else {
	return -1;
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
HIDDEN const char *
type_to_str(const struct rt_db_internal *obj, int arb_type) {
    if (arb_type) return arb_type_to_str(arb_type);
    return obj->idb_meth->ft_label;
}

void
diff_init_avp(struct diff_avp *attr_result)
{
   if (!attr_result) return;
   attr_result->name = NULL;
   attr_result->left_value = NULL;
   attr_result->ancestor_value = NULL;
   attr_result->right_value = NULL;
}

void
diff_free_avp(struct diff_avp *attr_result)
{
   if (!attr_result) return;
   if (attr_result->name) bu_free(attr_result->name, "free diff_avp name");
   if (attr_result->left_value) bu_free(attr_result->left_value, "free diff_avp left_value");
   if (attr_result->ancestor_value) bu_free(attr_result->ancestor_value, "free diff_avp ancestor_value");
   if (attr_result->right_value) bu_free(attr_result->right_value, "free diff_avp right_value");
}

void
diff_init_result(struct diff_result *result, const struct bn_tol *curr_diff_tol, const char *obj_name)
{
    if (!result) return;
    if (obj_name) {
	result->obj_name = bu_strdup(obj_name);
    } else {
	result->obj_name = NULL;
    }
    result->dp_left = RT_DIR_NULL;
    result->dp_ancestor = RT_DIR_NULL;
    result->dp_right = RT_DIR_NULL;
    result->param_state = DIFF_EMPTY;
    result->attr_state = DIFF_EMPTY;
    BU_GET(result->diff_tol, struct bn_tol);
    if (curr_diff_tol) {
	(result)->diff_tol->magic = BN_TOL_MAGIC;
	(result)->diff_tol->dist = curr_diff_tol->dist;
	(result)->diff_tol->dist_sq = curr_diff_tol->dist_sq;
	(result)->diff_tol->perp = curr_diff_tol->perp;
	(result)->diff_tol->para = curr_diff_tol->para;
    } else {
	BN_TOL_INIT(result->diff_tol);
    }
    BU_GET(result->param_diffs, struct bu_ptbl);
    BU_GET(result->attr_diffs, struct bu_ptbl);
    BU_PTBL_INIT(result->param_diffs);
    BU_PTBL_INIT(result->attr_diffs);
}


void
diff_free_result(struct diff_result *result)
{
    unsigned int i = 0;
    if (result->obj_name) {
	bu_free(result->obj_name, "free name copy in diff result");
    }
    BU_PUT(result->diff_tol, struct bn_tol);
    for (i = 0; i < BU_PTBL_LEN(result->param_diffs); i++) {
	struct diff_avp *avp = (struct diff_avp *)BU_PTBL_GET(result->param_diffs, i);
	diff_free_avp(avp);
	BU_PUT(avp, struct diff_avp);
    }
    bu_ptbl_free(result->param_diffs);
    BU_PUT(result->param_diffs, struct bu_ptbl);
    for (i = 0; i < BU_PTBL_LEN(result->attr_diffs); i++) {
	struct diff_avp *avp = (struct diff_avp *)BU_PTBL_GET(result->attr_diffs, i);
	diff_free_avp(avp);
	BU_PUT(avp, struct diff_avp);
    }
    bu_ptbl_free(result->attr_diffs);
    BU_PUT(result->attr_diffs, struct bu_ptbl);
}

HIDDEN int
avpp_val_compare(const char *val1, const char *val2, const struct bn_tol *diff_tol)
{
    /* We need to look for numbers to do tolerance based comparisons */
    int num_compare = 1;
    int color_compare = 1;
    int pnt_compare = 1;
    double dval1, dval2;
    int c1val1, c1val2, c1val3;
    int c2val1, c2val2, c2val3;
    float p1val1, p1val2, p1val3;
    float p2val1, p2val2, p2val3;
    char *endptr;
    /* Don't try a numerical comparison unless the strings differ -
     * numerical attempts when they are not needed can introduce
     * invalid changes */
    int retval = BU_STR_EQUAL(val1, val2);

    if (!retval) {
	/* First, check for individual numbers */
	errno = 0;
	dval1 = strtod(val1, &endptr);
	if (errno == EINVAL || *endptr != '\0') num_compare--;
	errno = 0;
	dval2 = strtod(val2, &endptr);
	if (errno == EINVAL || *endptr != '\0') num_compare--;
	if (num_compare == 1) {return NEAR_EQUAL(dval1, dval2, diff_tol->dist);}

	/* If we didn't find numbers, try for colors (3 integer numbers) */
	if (sscanf(val1, "%d %d %d", &c1val1, &c1val2, &c1val3) == 3) color_compare--;
	if (sscanf(val2, "%d %d %d", &c2val1, &c2val2, &c2val3) == 3) color_compare--;
	if (color_compare == 1) return retval;

	/* If we didn't find numbers, try for points (3 floating point numbers) */
	if (sscanf(val1, "%f %f %f", &p1val1, &p1val2, &p1val3) == 3) pnt_compare--;
	if (sscanf(val2, "%f %f %f", &p2val1, &p2val2, &p2val3) == 3) pnt_compare--;

	if (pnt_compare == 1) {
	    vect_t v1, v2;
	    VSET(v1, p1val1, p1val2, p1val3);
	    VSET(v2, p2val1, p2val2, p2val3);
	    return VNEAR_EQUAL(v1, v2, diff_tol->dist);
	}
    }
    return retval;
}

int
db_avs_diff(const struct bu_attribute_value_set *left_set,
	    const struct bu_attribute_value_set *right_set,
            const struct bn_tol *diff_tol,
	    int (*add_func)(const char *attr_name, const char *attr_val, void *data),
	    int (*del_func)(const char *attr_name, const char *attr_val, void *data),
	    int (*chgd_func)(const char *attr_name, const char *attr_val_left, const char *attr_val_right, void *data),
	    int (*unchgd_func)(const char *attr_name, const char *attr_val, void *data),
	    void *client_data)
{
    int state = DIFF_EMPTY;
    struct bu_attribute_value_pair *avp;
    for (BU_AVS_FOR(avp, left_set)) {
	const char *val2 = bu_avs_get(right_set, avp->name);
	if (!val2) {
	    if (del_func) {state |= del_func(avp->name, avp->value, client_data);}
	} else {
	    if (avpp_val_compare(avp->value, val2, diff_tol)) {
		if (unchgd_func) {state |= unchgd_func(avp->name, avp->value, client_data);}
	    } else {
		if (chgd_func) {state |= chgd_func(avp->name, avp->value, val2, client_data);}
	    }
	}
    }
    for (BU_AVS_FOR(avp, right_set)) {
	const char *val1 = bu_avs_get(left_set, avp->name);
	if (!val1) {
	    if (add_func) {state |= add_func(avp->name, avp->value, client_data);}
	}
    }
    return state;
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

struct diff_elements {
    const char *name;
    int bin_obj;
    struct rt_db_internal *intern;
    int bin_params;
    void *idb_ptr;
    struct bu_attribute_value_set *params;
    struct bu_attribute_value_set *attrs;
};

HIDDEN void
get_diff_components(struct diff_elements *el, const struct db_i *dbip, const struct directory *dp)
{
    el->name = NULL;
    el->idb_ptr = NULL;
    el->bin_params = 0;
    el->bin_obj = 0;
    el->intern = NULL;

    BU_GET(el->attrs, struct bu_attribute_value_set);
    BU_AVS_INIT(el->attrs);
    BU_GET(el->params, struct bu_attribute_value_set);
    BU_AVS_INIT(el->params);

    if (!dp) return;

    el->name = dp->d_namep;

    /* Deal with attribute-only objects, since they're "special" */
    if (dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	struct bu_external dp_ext;
	struct db5_raw_internal dp_raw;
	BU_EXTERNAL_INIT(&dp_ext);
	if (db_get_external(&dp_ext, dp, dbip) < 0 || db5_get_raw_internal_ptr(&dp_raw, dp_ext.ext_buf) == NULL) {
	    bu_free_external(&dp_ext);
	    el->bin_obj = 1;
	    return;
	}
	/* Parse out the attributes */
	if (db5_import_attributes(el->attrs, &dp_raw.attributes) < 0) {
	    bu_free_external(&dp_ext);
	    el->bin_obj = 1;
	    return;
	}
	bu_free_external(&dp_ext);
	return;
    }

    /* Now deal with more normal objects */
    BU_GET(el->intern, struct rt_db_internal);
    RT_DB_INTERNAL_INIT(el->intern);
    if (rt_db_get_internal(el->intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	/* Arrgh - No internal representation */
	rt_db_free_internal(el->intern);
	BU_PUT(el->intern, struct rt_db_internal);
	el->intern = NULL;
	el->bin_obj = 1;
	return;
    } else {
	struct bu_vls s_tcl = BU_VLS_INIT_ZERO;
	int have_tcl = 1;

	el->idb_ptr = el->intern->idb_ptr;

	/* object type isn't a normal parameter attribute, so add it as such */
	if (el->intern->idb_minor_type == DB5_MINORTYPE_BRLCAD_ARB8) {
	    struct bn_tol arb_tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1.0 - 1e-6 };
	    bu_avs_add(el->params, "DB5_MINORTYPE", type_to_str(el->intern, rt_arb_std_type(el->intern, &arb_tol)));
	} else {
	    bu_avs_add(el->params, "DB5_MINORTYPE", el->intern->idb_meth->ft_label);
	}

	/* Convert the rest of the params to attributes, if possible */
	bu_vls_trunc(&s_tcl, 0);
	if (el->intern->idb_meth->ft_get(&s_tcl, el->intern, NULL) == BRLCAD_ERROR) have_tcl = 0;
	if (!have_tcl || tcl_list_to_avs(bu_vls_addr(&s_tcl), el->params, 1)) have_tcl = 0;
	if (!have_tcl) {
	    el->bin_params = 1;
	}
	bu_vls_free(&s_tcl);

	/* Pick up the extra attributes */
	if (el->intern->idb_avs.magic == BU_AVS_MAGIC) {
	    bu_avs_merge(el->attrs, &(el->intern->idb_avs));
	}

    }
}

HIDDEN void
free_diff_components(struct diff_elements *el)
{
    bu_avs_free(el->attrs);
    bu_avs_free(el->params);
    BU_PUT(el->attrs, struct bu_attribute_value_set);
    BU_PUT(el->params, struct bu_attribute_value_set);
    if (el->intern) {
	rt_db_free_internal(el->intern);
	BU_PUT(el->intern, struct rt_db_internal);
    }
}

HIDDEN int
diff_dp_attr_add(const char *attr_name, const char *attr_val, void *data)
{
    struct bu_ptbl *diffs = (struct bu_ptbl *)data;
    struct diff_avp *avp;
    BU_GET(avp, struct diff_avp);
    diff_init_avp(avp);
    avp->state = DIFF_ADDED;
    avp->name = bu_strdup(attr_name);
    avp->right_value = bu_strdup(attr_val);
    bu_ptbl_ins(diffs, (long *)avp);
    return avp->state;
}

HIDDEN int
diff_dp_attr_del(const char *attr_name, const char *attr_val, void *data)
{
    struct bu_ptbl *diffs = (struct bu_ptbl *)data;
    struct diff_avp *avp;
    BU_GET(avp, struct diff_avp);
    diff_init_avp(avp);
    avp->state = DIFF_REMOVED;
    avp->name = bu_strdup(attr_name);
    avp->left_value = bu_strdup(attr_val);
    bu_ptbl_ins(diffs, (long *)avp);
    return avp->state;
}

HIDDEN int
diff_dp_attr_chgd(const char *attr_name, const char *attr_val_left, const char *attr_val_right, void *data)
{
    struct bu_ptbl *diffs = (struct bu_ptbl *)data;
    struct diff_avp *avp;
    BU_GET(avp, struct diff_avp);
    diff_init_avp(avp);
    avp->state = DIFF_CHANGED;
    avp->name = bu_strdup(attr_name);
    avp->left_value = bu_strdup(attr_val_left);
    avp->right_value = bu_strdup(attr_val_right);
    bu_ptbl_ins(diffs, (long *)avp);
    return avp->state;
}

HIDDEN int
diff_dp_attr_unchgd(const char *attr_name, const char *attr_val, void *data)
{
    struct bu_ptbl *diffs = (struct bu_ptbl *)data;
    struct diff_avp *avp;
    BU_GET(avp, struct diff_avp);
    diff_init_avp(avp);
    avp->state = DIFF_UNCHANGED;
    avp->name = bu_strdup(attr_name);
    avp->left_value = bu_strdup(attr_val);
    avp->right_value = bu_strdup(attr_val);
    bu_ptbl_ins(diffs, (long *)avp);
    return avp->state;
}

int
db_diff_dp(const struct db_i *left,
	const struct db_i *right,
	const struct directory *left_dp,
       	const struct directory *right_dp,
       	const struct bn_tol *diff_tol,
       	db_compare_criteria_t flags,
	struct diff_result *ext_result)
{
    int state = DIFF_EMPTY;

    struct diff_elements left_components;
    struct diff_elements right_components;

    struct diff_result *result = NULL;

    if (!left_dp && !right_dp) return -1;
    if (!diff_tol) return -1;

    /* If we aren't populating a result struct for the
     * caller, use a local copy to keep the code simple */
    if (!ext_result) {
	BU_GET(result, struct diff_result);
	diff_init_result(result, diff_tol, "tmp");
    } else {
	result = ext_result;
    }

    if (left_dp) result->dp_left = left_dp;
    if (right_dp) result->dp_right = right_dp;

    get_diff_components(&left_components, left, left_dp);
    get_diff_components(&right_components, right, right_dp);

    if (flags == DB_COMPARE_ALL || flags & DB_COMPARE_PARAM) {

	result->param_state |= db_avs_diff(left_components.params, right_components.params, diff_tol, diff_dp_attr_add, diff_dp_attr_del, diff_dp_attr_chgd, diff_dp_attr_unchgd, (void *)(result->param_diffs));
	/*compare the idb_ptr memory, if the types are the same.*/
	if (left_components.bin_params && right_components.bin_params && left_components.idb_ptr && right_components.idb_ptr) {
	    if (left_components.intern->idb_minor_type == right_components.intern->idb_minor_type) {
		int memsize = rt_intern_struct_size(left_components.intern->idb_minor_type);
		if (memcmp((void *)left_components.idb_ptr, (void *)right_components.idb_ptr, memsize)) {
		    /* If we didn't pick up differences in the avs comparison, we need to use this result to flag a parameter difference */
		    if (result->param_state == DIFF_UNCHANGED || result->param_state == DIFF_EMPTY) result->param_state |= DIFF_CHANGED;
		} else {
		    if (result->param_state == DIFF_EMPTY) result->param_state |= DIFF_UNCHANGED;
		}
	    }
	}
    }

    if (flags == DB_COMPARE_ALL || flags & DB_COMPARE_ATTRS) {
	result->attr_state |= db_avs_diff(left_components.attrs, right_components.attrs, diff_tol, diff_dp_attr_add, diff_dp_attr_del, diff_dp_attr_chgd, diff_dp_attr_unchgd, (void *)(result->attr_diffs));
    }

    free_diff_components(&left_components);
    free_diff_components(&right_components);

    state |= result->param_state;
    state |= result->attr_state;

    if (!ext_result) {
	diff_free_result(result);
	BU_PUT(result, struct diff_result);
    }

    return state;
}

int
db_diff(const struct db_i *dbip1,
	const struct db_i *dbip2,
	const struct bn_tol *diff_tol,
	db_compare_criteria_t flags,
	struct bu_ptbl *results)
{
    int state = DIFF_EMPTY;
    struct directory *dp1, *dp2;

    /* look at all objects in this database */
    FOR_ALL_DIRECTORY_START(dp1, dbip1) {
	struct bu_external ext1, ext2;
	int extern_state = DIFF_UNCHANGED;
	struct diff_result *result;
	BU_GET(result, struct diff_result);
	diff_init_result(result, diff_tol, dp1->d_namep);

	/* determine the status of this object in the other database */
	dp2 = db_lookup(dbip2, dp1->d_namep, 0);

	/* If we're checking everything, we want a sanity check to make sure we spot it when the objects differ */
	if (flags == DB_COMPARE_ALL && dp1 != RT_DIR_NULL && dp1->d_major_type != DB5_MAJORTYPE_ATTRIBUTE_ONLY && dp2 != RT_DIR_NULL && dp2->d_major_type != DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	    if (db_get_external(&ext1, dp1, dbip1) || db_get_external(&ext2, dp2, dbip2)) {
		bu_log("Warning - Error getting bu_external form when comparing %s and %s\n", dp1->d_namep, dp2->d_namep);
	    } else {
		if (db_diff_external(&ext1, &ext2)) {extern_state = DIFF_CHANGED;}
	    }
	    bu_free_external(&ext1);
	    bu_free_external(&ext2);
	}

	/* Do internal diffs */
	if (flags == DB_COMPARE_ALL || flags & DB_COMPARE_PARAM) {
	    state |= db_diff_dp(dbip1, dbip2, dp1, dp2, diff_tol, DB_COMPARE_PARAM, result);
	}
	if (flags == DB_COMPARE_ALL || flags & DB_COMPARE_ATTRS) {
	    state |= db_diff_dp(dbip1, dbip2, dp1, dp2, diff_tol, DB_COMPARE_ATTRS, result);
	}
	if (flags == DB_COMPARE_ALL && state == DIFF_UNCHANGED && extern_state == DIFF_CHANGED) {
	    bu_log("Warning - internal comparison and bu_external comparison disagree when comparing %s and %s\n", dp1->d_namep, dp2->d_namep);

	}

	if (results) {
	    bu_ptbl_ins(results, (long *)result);
	} else {
	    diff_free_result(result);
	    BU_PUT(result, struct diff_result);
	}

    } FOR_ALL_DIRECTORY_END;

    /* now look for objects in the other database that aren't here */
    FOR_ALL_DIRECTORY_START(dp2, dbip2) {

	/* determine the status of this object in the other database */
	dp1 = db_lookup(dbip1, dp2->d_namep, 0);

	/* By this point, any differences will be additions */
	if (dp1 == RT_DIR_NULL) {
	    struct diff_result *result;
	    BU_GET(result, struct diff_result);
	    diff_init_result(result, diff_tol, dp2->d_namep);

	    /* Do internal diffs */
	    if (flags == DB_COMPARE_ALL || flags & DB_COMPARE_PARAM) {
		state |= db_diff_dp(dbip1, dbip2, dp1, dp2, diff_tol, DB_COMPARE_PARAM, result);
	    }
	    if (flags == DB_COMPARE_ALL || flags & DB_COMPARE_ATTRS) {
		state |= db_diff_dp(dbip1, dbip2, dp1, dp2, diff_tol, DB_COMPARE_ATTRS, result);
	    }

	    if (results) {
		bu_ptbl_ins(results, (long *)result);
	    } else {
		diff_free_result(result);
		BU_PUT(result, struct diff_result);
	    }
	}

    } FOR_ALL_DIRECTORY_END;

    return state;
}

int
db_avs_diff3(const struct bu_attribute_value_set *left_set,
	    const struct bu_attribute_value_set *ancestor_set,
	    const struct bu_attribute_value_set *right_set,
            const struct bn_tol *diff_tol,
	    int (*add_func)(const char *attr_name, const char *attr_val_left, const char *attr_val_right, void *data),
	    int (*del_func)(const char *attr_name, const char *attr_val_left, const char *attr_val_ancestor, const char *attr_val_right, void *data),
	    int (*chgd_func)(const char *attr_name, const char *attr_val_left, const char *attr_val_ancestor, const char *attr_val_right, void *data),
	    int (*conflict_func)(const char *attr_name, const char *attr_val_left, const char *attr_val_ancestor, const char *attr_val_right, void *data),
	    int (*unchgd_func)(const char *attr_name, const char *attr_val, void *data),
	    void *client_data)
{
    int state = DIFF_EMPTY;
    struct bu_attribute_value_pair *avp;
    for (BU_AVS_FOR(avp, ancestor_set)) {
	const char *val_ancestor = bu_avs_get(ancestor_set, avp->name);
	const char *val_left = bu_avs_get(left_set, avp->name);
	const char *val_right = bu_avs_get(right_set, avp->name);

        /* The possibilities are:
         *
         * (!val_left && !val_right) && val_ancestor
         * (val_left && !val_right) && (val_ancestor == val_left)
         * (val_left && !val_right) && (val_ancestor != val_left)
         * (!val_left && val_right) && (val_ancestor == val_right)
         * (!val_left && val_right) && (val_ancestor != val_right)
         * (val_left == val_right) && (val_ancestor == val_left)
         * (val_left == val_right) && (val_ancestor != val_left)
         * (val_left != val_right) && (val_ancestor == val_left)
         * (val_left != val_right) && (val_ancestor == val_right)
         * (val_left != val_right) && (val_ancestor != val_left && val_ancestor != val_right)
         */

	if (!val_left || !val_right) {
	    /* Removed from both - no conflict, nothing to merge */
	    if ((!val_left && !val_right) && val_ancestor) {
		if (del_func) {state |= del_func(avp->name, NULL, avp->value, NULL, client_data);}
	    }

	    /* Removed from right_set only, left_set not changed - no conflict,
	     * right_set removal wins and left_set is not merged */
	    if ((val_left && !val_right) && avpp_val_compare(val_ancestor, val_left, diff_tol)) {
		if (del_func) {state |= del_func(avp->name, val_left, avp->value, NULL, client_data);}
	    }

	    /* Removed from right_set only, left_set changed - conflict */
	    if ((val_left && !val_right) && !avpp_val_compare(val_ancestor, val_left, diff_tol)) {
		if (conflict_func) {state |= conflict_func(avp->name, val_left, val_ancestor, val_right, client_data);}
	    }

	    /* Removed from left_set only, right_set not changed - no conflict,
	     * left_set change wins and right_set not merged */
	    if ((!val_left && val_right) && avpp_val_compare(val_ancestor, val_right, diff_tol)) {
		if (del_func) {state |= del_func(avp->name, NULL, avp->value, val_right, client_data);}
	    }

	    /* Removed from left_set only, right_set changed - conflict,
	     * merge defaults to preserving information */
	    if ((!val_left && val_right) && !avpp_val_compare(val_ancestor, val_right, diff_tol)) {
		if (conflict_func) {state |= conflict_func(avp->name, val_left, val_ancestor, val_right, client_data);}
	    }
	} else {

	    /* All values equal, unchanged and merged */
	    if (avpp_val_compare(val_left, val_right, diff_tol) && avpp_val_compare(val_ancestor, val_left, diff_tol)) {
		if (unchgd_func) {state |= unchgd_func(avp->name, avp->value, client_data);}
	    }
	    /* Identical change to both - changed and merged */
	    if (avpp_val_compare(val_left, val_right, diff_tol) && !avpp_val_compare(val_ancestor, val_left, diff_tol)) {
		if (chgd_func) {state |= chgd_func(avp->name, val_left, val_ancestor, val_right, client_data);}
	    }
	    /* val_right changed, val_left not changed - val_right change wins and is merged */
	    if (!avpp_val_compare(val_left, val_right, diff_tol) && avpp_val_compare(val_ancestor, val_left, diff_tol)) {
		if (chgd_func) {state |= chgd_func(avp->name, val_left, val_ancestor, val_right, client_data);}
	    }
	    /* val_left changed, val_right not changed - val_left change wins and is merged */
	    if (!avpp_val_compare(val_left, val_right, diff_tol) && avpp_val_compare(val_ancestor, val_right, diff_tol)) {
		if (chgd_func) {state |= chgd_func(avp->name, val_left, val_ancestor, val_right, client_data);}
	    }
	    /* val_left and val_right changed and incompatible - conflict,
	     * merge adds conflict a/v pairs */
	    if (!avpp_val_compare(val_left, val_right, diff_tol) && !avpp_val_compare(val_ancestor, val_left, diff_tol) && !avpp_val_compare(val_ancestor, val_right, diff_tol)) {
		if (conflict_func) {state |= conflict_func(avp->name, val_left, val_ancestor, val_right, client_data);}
	    }
	}
    }

    /* Now do left_set - anything in ancestor has already been handled */
    for (BU_AVS_FOR(avp, left_set)) {
        const char *val_ancestor = bu_avs_get(ancestor_set, avp->name);
        if (!val_ancestor) {
	    const char *val_left = bu_avs_get(left_set, avp->name);
            const char *val_right = bu_avs_get(right_set, avp->name);
	    /* (val_left && !val_right) */
	    if (val_left && !val_right) {
		if (add_func) {state |= add_func(avp->name, val_left, NULL, client_data);}
	    } else {
		int have_same_val = avpp_val_compare(val_left,val_right, diff_tol);
		/* (val_left == val_right) */
		if (have_same_val) {
		    if (add_func) {state |= add_func(avp->name, val_left, val_right, client_data);}
		} else {
		    /* (val_left != val_right) */
		    if (conflict_func) {state |= conflict_func(avp->name, val_left, NULL, val_right, client_data);}
		}
	    }
        }
    }

    /* Last but not least, right_set - anything in ancestor and/or left_set has already been handled */
    for (BU_AVS_FOR(avp, right_set)) {
        if (!bu_avs_get(ancestor_set, avp->name) && !bu_avs_get(left_set, avp->name)) {
	    if (add_func) {state |= add_func(avp->name, NULL, avp->value, client_data);}
	}
    }

    return state;
}

HIDDEN int
diff3_dp_attr_add(const char *attr_name, const char *attr_val_left, const char *attr_val_right, void *data)
{
    struct bu_ptbl *diffs = (struct bu_ptbl *)data;
    struct diff_avp *avp;
    BU_GET(avp, struct diff_avp);
    diff_init_avp(avp);
    avp->name = bu_strdup(attr_name);
    avp->state = DIFF_ADDED;
    if (attr_val_left) avp->left_value = bu_strdup(attr_val_left);
    if (attr_val_right) avp->right_value = bu_strdup(attr_val_right);
    bu_ptbl_ins(diffs, (long *)avp);
    return avp->state;
}

HIDDEN int
diff3_dp_attr_del(const char *attr_name, const char *attr_val_left, const char *attr_val_ancestor, const char *attr_val_right, void *data)
{
    struct bu_ptbl *diffs = (struct bu_ptbl *)data;
    struct diff_avp *avp;
    BU_GET(avp, struct diff_avp);
    diff_init_avp(avp);
    avp->name = bu_strdup(attr_name);
    avp->state = DIFF_REMOVED;
    avp->ancestor_value = bu_strdup(attr_val_ancestor);
    if (attr_val_right)
	avp->right_value = bu_strdup(attr_val_right);
    if (attr_val_left)
	avp->left_value = bu_strdup(attr_val_left);
    bu_ptbl_ins(diffs, (long *)avp);
    return avp->state;
}

HIDDEN int
diff3_dp_attr_chgd(const char *attr_name, const char *attr_val_left, const char *attr_val_ancestor, const char *attr_val_right, void *data)
{
    struct bu_ptbl *diffs = (struct bu_ptbl *)data;
    struct diff_avp *avp;
    BU_GET(avp, struct diff_avp);
    diff_init_avp(avp);
    avp->name = bu_strdup(attr_name);
    avp->state = DIFF_CHANGED;
    avp->ancestor_value = bu_strdup(attr_val_ancestor);
    avp->right_value = bu_strdup(attr_val_right);
    avp->left_value = bu_strdup(attr_val_left);
    bu_ptbl_ins(diffs, (long *)avp);
    return avp->state;
}

HIDDEN int
diff3_dp_attr_conflict(const char *attr_name, const char *attr_val_left, const char *attr_val_ancestor, const char *attr_val_right, void *data)
{
    struct bu_ptbl *diffs = (struct bu_ptbl *)data;
    struct diff_avp *avp;
    BU_GET(avp, struct diff_avp);
    diff_init_avp(avp);
    avp->name = bu_strdup(attr_name);
    avp->state = DIFF_CONFLICT;
    if (attr_val_ancestor) avp->ancestor_value = bu_strdup(attr_val_ancestor);
    if (attr_val_right) avp->right_value = bu_strdup(attr_val_right);
    if (attr_val_left) avp->left_value = bu_strdup(attr_val_left);
    bu_ptbl_ins(diffs, (long *)avp);
    return avp->state;
}

HIDDEN int
diff3_dp_attr_unchgd(const char *attr_name, const char *attr_val, void *data)
{
    struct bu_ptbl *diffs = (struct bu_ptbl *)data;
    struct diff_avp *avp;
    BU_GET(avp, struct diff_avp);
    diff_init_avp(avp);
    avp->state = DIFF_UNCHANGED;
    avp->name = bu_strdup(attr_name);
    avp->ancestor_value = bu_strdup(attr_val);
    avp->right_value = bu_strdup(attr_val);
    avp->left_value = bu_strdup(attr_val);
    bu_ptbl_ins(diffs, (long *)avp);
    return avp->state;
}

int
db_diff3_dp(const struct db_i *left,
	const struct db_i *ancestor,
	const struct db_i *right,
	const struct directory *left_dp,
	const struct directory *ancestor_dp,
       	const struct directory *right_dp,
       	const struct bn_tol *diff3_tol,
       	db_compare_criteria_t flags,
	struct diff_result *ext_result)
{
    int state = DIFF_EMPTY;

    struct diff_elements left_components;
    struct diff_elements ancestor_components;
    struct diff_elements right_components;

    struct diff_result *result = NULL;

    if (left == DBI_NULL && ancestor == DBI_NULL && right == DBI_NULL) return -1;
    if (left_dp == RT_DIR_NULL && ancestor_dp == RT_DIR_NULL && right_dp == RT_DIR_NULL) return -1;
    if (!diff3_tol) return -1;

    /* If we aren't populating a result struct for the
     * caller, use a local copy to keep the code simple */
    if (!ext_result) {
	BU_GET(result, struct diff_result);
	diff_init_result(result, diff3_tol, "tmp");
    } else {
	result = ext_result;
    }

    if (left_dp) result->dp_left = left_dp;
    if (ancestor_dp) result->dp_ancestor = ancestor_dp;
    if (right_dp) result->dp_right = right_dp;

    get_diff_components(&left_components, left, left_dp);
    get_diff_components(&ancestor_components, ancestor, ancestor_dp);
    get_diff_components(&right_components, right, right_dp);

    if (flags == DB_COMPARE_ALL || flags & DB_COMPARE_PARAM) {

	result->param_state |= db_avs_diff3(left_components.params, ancestor_components.params, right_components.params,
		diff3_tol, diff3_dp_attr_add, diff3_dp_attr_del, diff3_dp_attr_chgd, diff3_dp_attr_conflict,
		diff3_dp_attr_unchgd, (void *)(result->param_diffs));
	/*compare the idb_ptr memory, if the types are the same.*/
	if (left_components.bin_params && ancestor_components.bin_params && right_components.bin_params)
	   if (left_components.idb_ptr && ancestor_components.idb_ptr && right_components.idb_ptr) {
	    if ((left_components.intern->idb_minor_type == ancestor_components.intern->idb_minor_type) &&
		    (left_components.intern->idb_minor_type == right_components.intern->idb_minor_type)) {
		int memsize = rt_intern_struct_size(left_components.intern->idb_minor_type);
		if (memcmp((void *)left_components.idb_ptr, (void *)right_components.idb_ptr, memsize) &&
			memcmp((void *)ancestor_components.idb_ptr, (void *)right_components.idb_ptr, memsize)) {
		    /* If we didn't pick up differences in the avs comparison, we need to use this result to flag a parameter difference */
		    if (result->param_state == DIFF_UNCHANGED || result->param_state == DIFF_EMPTY) result->param_state |= DIFF_CHANGED;
		} else {
		    if (result->param_state == DIFF_EMPTY) result->param_state |= DIFF_UNCHANGED;
		}
	    }
	}
    }

    if (flags == DB_COMPARE_ALL || flags & DB_COMPARE_ATTRS) {
	result->param_state |= db_avs_diff3(left_components.attrs, ancestor_components.attrs, right_components.attrs,
		diff3_tol, diff3_dp_attr_add, diff3_dp_attr_del, diff3_dp_attr_chgd, diff3_dp_attr_conflict,
		diff3_dp_attr_unchgd, (void *)(result->attr_diffs));
    }

    free_diff_components(&left_components);
    free_diff_components(&ancestor_components);
    free_diff_components(&right_components);

    state |= result->param_state;
    state |= result->attr_state;

    if (!ext_result) {
	diff_free_result(result);
	BU_PUT(result, struct diff_result);
    }

    return state;
}

int
db_diff3(const struct db_i *dbip_left,
	const struct db_i *dbip_ancestor,
	const struct db_i *dbip_right,
	const struct bn_tol *diff3_tol,
	db_compare_criteria_t flags,
	struct bu_ptbl *results)
{
    int state = DIFF_EMPTY;
    struct directory *dp_ancestor, *dp_left, *dp_right;

    /* Step 1: look at all objects in the ancestor database */
    FOR_ALL_DIRECTORY_START(dp_ancestor, dbip_ancestor) {
	struct bu_external ext_left, ext_ancestor, ext_right;
	struct diff_result *result;
	int ancestor_state = DIFF_EMPTY;
	int extern_state = DIFF_UNCHANGED;
	BU_GET(result, struct diff_result);
	diff_init_result(result, diff3_tol, dp_ancestor->d_namep);

	dp_left = db_lookup(dbip_left, dp_ancestor->d_namep, 0);
	dp_right = db_lookup(dbip_right, dp_ancestor->d_namep, 0);

	/* If we're checking everything, we want a sanity check to make sure we spot it when the objects differ */
	if (flags == DB_COMPARE_ALL &&
	       	dp_left != RT_DIR_NULL && dp_left->d_major_type != DB5_MAJORTYPE_ATTRIBUTE_ONLY &&
	       	dp_ancestor != RT_DIR_NULL && dp_ancestor->d_major_type != DB5_MAJORTYPE_ATTRIBUTE_ONLY &&
	       	dp_right != RT_DIR_NULL && dp_right->d_major_type != DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	    if (db_get_external(&ext_left, dp_left, dbip_left) || db_get_external(&ext_ancestor, dp_ancestor, dbip_ancestor) || db_get_external(&ext_right, dp_right, dbip_right)) {
		bu_log("Warning - Error getting bu_external form when comparing %s and %s\n", dp_left->d_namep, dp_ancestor->d_namep);
	    } else {
		if (db_diff_external(&ext_left, &ext_right) || db_diff_external(&ext_left, &ext_ancestor)) {extern_state = DIFF_CHANGED;}
	    }
	    bu_free_external(&ext_left);
	    bu_free_external(&ext_ancestor);
	    bu_free_external(&ext_right);
	}

	/* Do internal diffs */
	if (flags == DB_COMPARE_ALL || flags & DB_COMPARE_PARAM) {
	    ancestor_state |= db_diff3_dp(dbip_left, dbip_ancestor, dbip_right, dp_left, dp_ancestor, dp_right, diff3_tol, DB_COMPARE_PARAM, result);
	}
	if (flags == DB_COMPARE_ALL || flags & DB_COMPARE_ATTRS) {
	    ancestor_state |= db_diff3_dp(dbip_left, dbip_ancestor, dbip_right, dp_left, dp_ancestor, dp_right, diff3_tol, DB_COMPARE_ATTRS, result);
	}
	if (flags == DB_COMPARE_ALL && (ancestor_state == DIFF_UNCHANGED || ancestor_state == DIFF_EMPTY) && extern_state == DIFF_CHANGED) {
	    bu_log("Warning - internal comparison and bu_external comparison disagree\n");
	}

	if (results) {
	    bu_ptbl_ins(results, (long *)result);
	} else {
	    diff_free_result(result);
	    BU_PUT(result, struct diff_result);
	}
	state |= ancestor_state;

    } FOR_ALL_DIRECTORY_END;

    FOR_ALL_DIRECTORY_START(dp_left, dbip_left) {
	dp_ancestor = db_lookup(dbip_ancestor, dp_left->d_namep, 0);
	if (dp_ancestor == RT_DIR_NULL) {
	    struct bu_external ext_left, ext_right;
	    int left_state = DIFF_EMPTY;
	    int extern_state = DIFF_UNCHANGED;
	    struct diff_result *result;
	    BU_GET(result, struct diff_result);
	    diff_init_result(result, diff3_tol, dp_left->d_namep);

	    dp_right = db_lookup(dbip_right, dp_left->d_namep, 0);

	    /* If we're checking everything, we want a sanity check to make sure we spot it when the objects differ */
	    if (flags == DB_COMPARE_ALL &&
		    dp_left != RT_DIR_NULL && dp_left->d_major_type != DB5_MAJORTYPE_ATTRIBUTE_ONLY &&
		    dp_right != RT_DIR_NULL && dp_right->d_major_type != DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
		if (db_get_external(&ext_left, dp_left, dbip_left) || db_get_external(&ext_right, dp_right, dbip_right)) {
		    bu_log("Warning - Error getting bu_external form\n");
		} else {
		    if (db_diff_external(&ext_left, &ext_right)) {extern_state = DIFF_CHANGED;}
		}
		bu_free_external(&ext_left);
		bu_free_external(&ext_right);
	    }

	    /* Do internal diffs */
	    if (flags == DB_COMPARE_ALL || flags & DB_COMPARE_PARAM) {
		left_state |= db_diff3_dp(dbip_left, dbip_ancestor, dbip_right, dp_left, NULL, dp_right, diff3_tol, DB_COMPARE_PARAM, result);
	    }
	    if (flags == DB_COMPARE_ALL || flags & DB_COMPARE_ATTRS) {
		left_state |= db_diff3_dp(dbip_left, dbip_ancestor, dbip_right, dp_left, NULL, dp_right, diff3_tol, DB_COMPARE_ATTRS, result);
	    }
	    if (flags == DB_COMPARE_ALL && (left_state == DIFF_UNCHANGED || left_state == DIFF_EMPTY) && extern_state == DIFF_CHANGED) {
		bu_log("Warning - internal comparison and bu_external comparison disagree");
	    }

	    if (results) {
		bu_ptbl_ins(results, (long *)result);
	    } else {
		diff_free_result(result);
		BU_PUT(result, struct diff_result);
	    }

	    state |= left_state;
	}

    } FOR_ALL_DIRECTORY_END;

    FOR_ALL_DIRECTORY_START(dp_right, dbip_right) {
	dp_ancestor = db_lookup(dbip_ancestor, dp_right->d_namep, 0);
	dp_left = db_lookup(dbip_left, dp_right->d_namep, 0);
	if (dp_ancestor == RT_DIR_NULL && dp_left == RT_DIR_NULL) {
	    struct diff_result *result;
	    BU_GET(result, struct diff_result);
	    diff_init_result(result, diff3_tol, dp_right->d_namep);

	    /* Do internal diffs */
	    if (flags == DB_COMPARE_ALL || flags & DB_COMPARE_PARAM) {
		state |= db_diff3_dp(dbip_left, dbip_ancestor, dbip_right, dp_left, dp_ancestor, dp_right, diff3_tol, DB_COMPARE_PARAM, result);
	    }
	    if (flags == DB_COMPARE_ALL || flags & DB_COMPARE_ATTRS) {
		state |= db_diff3_dp(dbip_left, dbip_ancestor, dbip_right, dp_left, dp_ancestor, dp_right, diff3_tol, DB_COMPARE_ATTRS, result);
	    }

	    if (results) {
		bu_ptbl_ins(results, (long *)result);
	    } else {
		diff_free_result(result);
		BU_PUT(result, struct diff_result);
	    }
	}
    } FOR_ALL_DIRECTORY_END;

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
