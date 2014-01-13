/*                        D I F F . C
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
/** @file g_diff.c
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
#include "bio.h"

#include "tcl.h"

#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "mater.h"
#include "analyze.h"

#if 0
HIDDEN int
compare_color_tables(struct bu_vls *diff_log, int mode, struct mater *mater_hd1, struct mater *mater_hd2)
{
    struct mater *mp1, *mp2;
    int found1 = 0, found2 = 0;
    int is_diff = 0;

    /* find a match for all color table entries of file1 in file2 */
    for (mp1 = mater_hd1; is_diff == 0 && mp1 != MATER_NULL; mp1 = mp1->mt_forw) {
	found1 = 0;
	mp2 = mater_hd2;
	while (mp2 != MATER_NULL) {
	    if (mp1->mt_low == mp2->mt_low
		    && mp1->mt_high == mp2->mt_high
		    && mp1->mt_r == mp2->mt_r
		    && mp1->mt_g == mp2->mt_g
		    && mp1->mt_b == mp2->mt_b)
	    {
		found1 = 1;
		break;
	    }
	    mp2 = mp2->mt_forw;
	}
	if (!found1) {
	    /* bu_log("could not find %d..%d: %d %d %d\n", mp1->mt_low, mp1->mt_high, mp1->mt_r, mp1->mt_g, mp1->mt_b); */
	    is_diff = 1;
	}
    }

    /* find a match for all color table entries of file2 in file1 */
    for (mp2 = mater_hd2; is_diff == 0 && mp2 != MATER_NULL; mp2 = mp2->mt_forw) {
	found2 = 0;
	mp1 = mater_hd1;
	while (mp1 != MATER_NULL) {
	    if (mp1->mt_low == mp2->mt_low
		    && mp1->mt_high == mp2->mt_high
		    && mp1->mt_r == mp2->mt_r
		    && mp1->mt_g == mp2->mt_g
		    && mp1->mt_b == mp2->mt_b)
	    {
		found2 = 1;
		break;
	    }
	    mp1 = mp1->mt_forw;
	}
	if (!found2) {
	    /* bu_log("could not find %d..%d: %d %d %d\n", mp1->mt_low, mp1->mt_high, mp1->mt_r, mp1->mt_g, mp1->mt_b); */
	    is_diff = 1;
	}
    }

    if (is_diff == 0) {
	return 0;
    }

    if (mode == HUMAN) {
	bu_vls_printf(diff_log, "Color table has changed from:\n");
	for (mp1 = mater_hd1; mp1 != MATER_NULL; mp1 = mp1->mt_forw) {
	    bu_vls_printf(diff_log, "\t%d..%d %d %d %d\n", mp1->mt_low, mp1->mt_high,
		    mp1->mt_r, mp1->mt_g, mp1->mt_b);
	}
	bu_vls_printf(diff_log, "\t\tto:\n");
	for (mp2 = mater_hd2; mp2 != MATER_NULL; mp2 = mp2->mt_forw) {
	    bu_vls_printf(diff_log, "\t%d..%d %d %d %d\n", mp2->mt_low, mp2->mt_high,
		    mp2->mt_r, mp2->mt_g, mp2->mt_b);
	}
    } else {
	/* punt, just delete the existing colortable and print a new one */
	bu_vls_printf(diff_log, "attr rm _GLOBAL regionid_colortable\n");
	for (mp2 = mater_hd2; mp2 != MATER_NULL; mp2 = mp2->mt_forw) {
	    bu_vls_printf(diff_log, "color %d %d %d %d %d\n", mp2->mt_low, mp2->mt_high,
		    mp2->mt_r, mp2->mt_g, mp2->mt_b);
	}
    }
    return 1;
}

#endif

/* Status:
 * 0 - valid diff info;
 * 1 - problem during diffing;
 *
 * Diff types:
 *
 * 0 - no difference;
 * 1 - in original only;
 * 2 - in new only;
 * 3 - type difference;
 * 4 - binary-only difference;
 * 5 - parameter/attribute differences;
 */
struct gdiff_result {
    uint32_t diff_magic;
    int status;
    int diff_type;
    struct directory *dp_orig;
    struct directory *dp_new;
    struct rt_db_internal *intern_orig;
    struct rt_db_internal *intern_new;
    struct bu_attribute_value_set internal_shared;
    struct bu_attribute_value_set internal_orig_only;
    struct bu_attribute_value_set internal_new_only;
    struct bu_attribute_value_set internal_orig_diff;
    struct bu_attribute_value_set internal_new_diff;
    struct bu_attribute_value_set additional_shared;
    struct bu_attribute_value_set additional_orig_only;
    struct bu_attribute_value_set additional_new_only;
    struct bu_attribute_value_set additional_orig_diff;
    struct bu_attribute_value_set additional_new_diff;
};

#define GDIFF_MAGIC 0x64696666 /**< diff */
#define GDIFF_NULL ((struct gdiff_result *)0)
#define GDIFF_IS_INITIALIZED(_p) (((struct gdiff_result *)(_p) != GDIFF_NULL) && (_p)->diff_magic == GDIFF_MAGIC)

struct bu_attribute_value_set *
diff_result(void *result, diff_result_t result_type)
{
    struct gdiff_result *curr_result = (struct gdiff_result *)result;
    if (!result) return NULL;
    BU_CKMAG(curr_result, GDIFF_MAGIC, "struct gdiff_result");

    switch(result_type) {
	case DIFF_SHARED_PARAM:
	    return &(curr_result->internal_shared);
	    break;
	case DIFF_ORIG_ONLY_PARAM:
	    return &(curr_result->internal_orig_only);
	    break;
	case DIFF_NEW_ONLY_PARAM:
	    return &(curr_result->internal_new_only);
	    break;
	case DIFF_CHANGED_ORIG_PARAM:
	    return &(curr_result->internal_orig_diff);
	    break;
	case DIFF_CHANGED_NEW_PARAM:
	    return &(curr_result->internal_new_diff);
	    break;
	case DIFF_SHARED_ATTR:
	    return &(curr_result->additional_shared);
	    break;
	case DIFF_ORIG_ONLY_ATTR:
	    return &(curr_result->additional_orig_only);
	    break;
	case DIFF_NEW_ONLY_ATTR:
	    return &(curr_result->additional_new_only);
	    break;
	case DIFF_CHANGED_ORIG_ATTR:
	    return &(curr_result->additional_orig_diff);
	    break;
	case DIFF_CHANGED_NEW_ATTR:
	    return &(curr_result->additional_new_diff);
	    break;
	default:
	    bu_log("Error - unknown result type requested!\n");
	    return NULL;
	    break;
    }
}

diff_t
diff_type(void *result)
{
    struct gdiff_result *curr_result = (struct gdiff_result *)result;
    BU_CKMAG(curr_result, GDIFF_MAGIC, "struct gdiff_result");

    switch(curr_result->diff_type) {
	case 0:
	    return DIFF_NONE;
	    break;
	case 1:
	    return DIFF_REMOVED;
	    break;
	case 2:
	    return DIFF_ADDED;
	    break;
	case 3:
	    return DIFF_TYPECHANGE;
	    break;
	case 4:
	    return DIFF_BINARY;
	    break;
	case 5:
	    return DIFF;
	    break;
	default:
	    bu_log("Error - unknown type!\n");
	    return DIFF_NONE;
	    break;
    }
}

struct directory *
diff_info_dp(void *result, diff_obj_t obj_type)
{
    struct gdiff_result *curr_result = (struct gdiff_result *)result;
    BU_CKMAG(curr_result, GDIFF_MAGIC, "struct gdiff_result");

    switch(obj_type) {
	case DIFF_ORIG:
	    return curr_result->dp_orig;
	    break;
	case DIFF_NEW:
	    return curr_result->dp_new;
	    break;
	default:
	    bu_log("Error - unknown type!\n");
	    return NULL;
	    break;
    }
}


struct rt_db_internal *
diff_info_intern(void *result, diff_obj_t obj_type)
{
    struct gdiff_result *curr_result = (struct gdiff_result *)result;
    BU_CKMAG(curr_result, GDIFF_MAGIC, "struct gdiff_result");

    switch(obj_type) {
	case DIFF_ORIG:
	    return curr_result->intern_orig;
	    break;
	case DIFF_NEW:
	    return curr_result->intern_new;
	    break;
	default:
	    bu_log("Error - unknown type!\n");
	    return NULL;
	    break;
    }
}

int
diff_status(void *result)
{
    struct gdiff_result *curr_result = (struct gdiff_result *)result;
    BU_CKMAG(curr_result, GDIFF_MAGIC, "struct gdiff_result");
    return curr_result->status;
}



void
gdiff_init(struct gdiff_result *result)
{
    result->diff_magic = GDIFF_MAGIC;
    result->status = 0;
    result->diff_type = 0;
    result->dp_orig = NULL;
    result->dp_new = NULL;
    result->intern_orig = (struct rt_db_internal *)bu_calloc(1, sizeof(struct rt_db_internal), "intern_orig");
    result->intern_new = (struct rt_db_internal *)bu_calloc(1, sizeof(struct rt_db_internal), "intern_new");
    RT_DB_INTERNAL_INIT(result->intern_orig);
    RT_DB_INTERNAL_INIT(result->intern_new);
    BU_AVS_INIT(&result->internal_shared);
    BU_AVS_INIT(&result->internal_orig_only);
    BU_AVS_INIT(&result->internal_new_only);
    BU_AVS_INIT(&result->internal_orig_diff);
    BU_AVS_INIT(&result->internal_new_diff);
    BU_AVS_INIT(&result->additional_shared);
    BU_AVS_INIT(&result->additional_orig_only);
    BU_AVS_INIT(&result->additional_new_only);
    BU_AVS_INIT(&result->additional_orig_diff);
    BU_AVS_INIT(&result->additional_new_diff);
}

void
diff_free(void *result)
{
    struct gdiff_result *curr_result = (struct gdiff_result *)result;
    if (!result) return;
    BU_CKMAG(curr_result, GDIFF_MAGIC, "struct gdiff_result");

    rt_db_free_internal(curr_result->intern_orig);
    rt_db_free_internal(curr_result->intern_new);
    bu_avs_free(&curr_result->internal_shared);
    bu_avs_free(&curr_result->internal_orig_only);
    bu_avs_free(&curr_result->internal_new_only);
    bu_avs_free(&curr_result->internal_orig_diff);
    bu_avs_free(&curr_result->internal_new_diff);
    bu_avs_free(&curr_result->additional_shared);
    bu_avs_free(&curr_result->additional_orig_only);
    bu_avs_free(&curr_result->additional_new_only);
    bu_avs_free(&curr_result->additional_orig_diff);
    bu_avs_free(&curr_result->additional_new_diff);
}

void
diff_free_ptbl(struct bu_ptbl *results_table)
{
    int i = 0;
    for (i = 0; i < (int)BU_PTBL_LEN(results_table); i++) {
	struct gdiff_result *result = (struct gdiff_result *)BU_PTBL_GET(results_table, i);
	diff_free((void *)result);
    }
    bu_ptbl_free(results_table);
}

int
avpp_val_compare(const char *val1, const char *val2, struct bn_tol *tol)
{
    if (!tol) return BU_STR_EQUAL(val1, val2);

    return 0;
}

int
bu_avs_diff(struct bu_attribute_value_set *shared,
	struct bu_attribute_value_set *orig_only,
	struct bu_attribute_value_set *new_only,
	struct bu_attribute_value_set *orig_diff,
	struct bu_attribute_value_set *new_diff,
	const struct bu_attribute_value_set *avs1,
	const struct bu_attribute_value_set *avs2,
	struct bn_tol *tol)
{
    struct bu_attribute_value_pair *avp;
    int have_shared = 0;
    int have_orig_only = 0;
    int have_new_only = 0;
    int have_diff = 0;
    if (!BU_AVS_IS_INITIALIZED(shared)) BU_AVS_INIT(shared);
    if (!BU_AVS_IS_INITIALIZED(orig_only)) BU_AVS_INIT(orig_only);
    if (!BU_AVS_IS_INITIALIZED(new_only)) BU_AVS_INIT(new_only);
    if (!BU_AVS_IS_INITIALIZED(orig_diff)) BU_AVS_INIT(orig_diff);
    if (!BU_AVS_IS_INITIALIZED(new_diff)) BU_AVS_INIT(new_diff);
    for (BU_AVS_FOR(avp, avs1)) {
	const char *val2 = bu_avs_get(avs2, avp->name);
	if (!val2) {
	    bu_avs_add(orig_only, avp->name, avp->value);
	    have_orig_only++;
	}
	if (avpp_val_compare(avp->value, val2, tol)) {
	    bu_avs_add(shared, avp->name, avp->value);
	    have_shared++;
	} else {
	    bu_avs_add(orig_diff, avp->name, avp->value);
	    bu_avs_add(new_diff, avp->name, val2);
	    have_diff++;
	}
    }
    for (BU_AVS_FOR(avp, avs2)) {
	const char *val1 = bu_avs_get(avs1, avp->name);
	if (!val1) {
	    bu_avs_add(new_only, avp->name, avp->value);
	    have_new_only++;
	}
    }
    if (have_diff) return 5;
    if (!have_orig_only && !have_new_only) return 0;
    if (have_orig_only && !have_new_only && !have_shared) return 1;
    if (!have_orig_only && have_new_only && !have_shared) return 2;
    if (have_orig_only && !have_new_only && have_shared) return 3;
    if (!have_orig_only && have_new_only && have_shared) return 4;
    return 5;
}

int
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

void *
diff_dp(void *result_in, struct directory *dp1, struct directory *dp2,
	struct db_i *dbip1, struct db_i *dbip2)
{
    struct bu_attribute_value_set avs1, avs2;
    struct bu_vls s1_tcl = BU_VLS_INIT_ZERO;
    struct bu_vls s2_tcl = BU_VLS_INIT_ZERO;
    int have_tcl1 = 1;
    int have_tcl2 = 1;
    struct gdiff_result *result = NULL;
    if (!result_in) {
	result = (struct gdiff_result *)bu_calloc(1, sizeof(struct gdiff_result), "gdiff result struct");
    } else {
	result = (struct gdiff_result *)result_in;
    }

    if (!(GDIFF_IS_INITIALIZED(result))) gdiff_init(result);

    /* Get the internal objects */
    if (rt_db_get_internal(result->intern_orig, dp1, dbip1, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_log("rt_db_get_internal(%s) failure\n", dp1->d_namep);
	result->status = 1;
	return (void *)result;
    }
    if (rt_db_get_internal(result->intern_new, dp2, dbip2, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_log("rt_db_get_internal(%s) failure\n", dp2->d_namep);
	result->status = 1;
	return (void *)result;
    }

    /* Do some type based checking - this will make a difference in
     * subsequent decision trees */
    if (result->intern_orig->idb_minor_type != result->intern_new->idb_minor_type) {
	result->diff_type = 3;
    } else {
	if (result->intern_orig->idb_minor_type == DB5_MINORTYPE_BRLCAD_ARB8) {
	    struct bn_tol arb_tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1.0 - 1e-6 };
	    int arb_type_1 = rt_arb_std_type(result->intern_orig, &arb_tol);
	    int arb_type_2 = rt_arb_std_type(result->intern_new, &arb_tol);
	    if (arb_type_1 != arb_type_2) result->diff_type = 3;
	}
    }

    /* Try to create attribute/value set definitions for these
     * objects from their Tcl list definitions.  We use an
     * offset of one because for all objects the first entry
     * in the list is the type of the object, which a) isn't
     * an attribute/value pair and b) we can already get from
     * the C data structures */
    bu_vls_trunc(&s1_tcl, 0);
    if (result->intern_orig->idb_meth->ft_get(&s1_tcl, result->intern_orig, NULL) == BRLCAD_ERROR) have_tcl1 = 0;
    bu_vls_trunc(&s2_tcl, 0);
    if (result->intern_new->idb_meth->ft_get(&s2_tcl, result->intern_new, NULL) == BRLCAD_ERROR) have_tcl2 = 0;
    if (have_tcl1 && have_tcl2) {
	if (tcl_list_to_avs(bu_vls_addr(&s1_tcl), &avs1, 1)) have_tcl1 = 0;
	if (tcl_list_to_avs(bu_vls_addr(&s2_tcl), &avs2, 1)) have_tcl2 = 0;
    }

    /* If we have both avs sets, do the detailed comparison */
    if (have_tcl1 && have_tcl2) {
	int avs_diff_result = bu_avs_diff(&result->internal_shared, &result->internal_orig_only,
		&result->internal_new_only, &result->internal_orig_diff,
		&result->internal_new_diff, &avs1, &avs2, (struct bn_tol *)NULL);
	/* If we have a difference and we haven't already set a type, do so */
	if (avs_diff_result && !result->diff_type) result->diff_type = 5;
    } else {
	/* If we reach this point and don't have successful avs creation, we are reduced
	 * to comparing the binary serializations of the two objects.  This is not ideal
	 * in that it precludes a nuanced description of the differences, but it is at
	 * least able to detect them.  We don't do this comparison for two different
	 * object types, since they are already assumed to be different. */
	if (result->diff_type != 3) {
	    struct bu_external ext1, ext2;

	    if (db_get_external(&ext1, dp1, dbip1)) {
		bu_log("ERROR: db_get_external failed on solid %s in %s\n", dp1->d_namep, dbip1->dbi_filename);
		result->status = 1;
		return (void *)result;
	    }
	    if (db_get_external(&ext2, dp2, dbip2)) {
		bu_log("ERROR: db_get_external failed on solid %s in %s\n", dp2->d_namep, dbip2->dbi_filename);
		result->status = 1;
		return (void *)result;
	    }

	    if (ext1.ext_nbytes != ext2.ext_nbytes) {
		result->diff_type = 4;
	    }

	    if (!(result->diff_type != 4) &&
		    memcmp((void *)ext1.ext_buf, (void *)ext2.ext_buf, ext1.ext_nbytes)) {
		result->diff_type = 4;
	    }
	}
    }

    /* Look at the extra attributes as well */
    if (result->intern_orig->idb_avs.magic == BU_AVS_MAGIC &&
	    result->intern_new->idb_avs.magic == BU_AVS_MAGIC) {
	int avs_diff_result = bu_avs_diff(&result->additional_shared,
		&result->additional_orig_only,
		&result->additional_new_only, &result->additional_orig_diff,
		&result->additional_new_diff,
		&result->intern_orig->idb_avs,
		&result->intern_new->idb_avs,
		(struct bn_tol *)NULL);
	/* If we have a difference and we haven't already set a type, do so */
	if (avs_diff_result && !result->diff_type) result->diff_type = 5;
    } else {
	if (result->intern_orig->idb_avs.magic == BU_AVS_MAGIC) {
	    bu_avs_merge(&result->additional_orig_only, &result->intern_orig->idb_avs);
	    /* If we haven't already set a type, do so */
	    if (!result->diff_type) result->diff_type = 5;
	}
	if (result->intern_new->idb_avs.magic == BU_AVS_MAGIC) {
	    bu_avs_merge(&result->additional_new_only, &result->intern_new->idb_avs);
	    /* If we haven't already set a type, do so */
	    if (!result->diff_type) result->diff_type = 5;
	}
    }

    bu_avs_free(&avs1);
    bu_avs_free(&avs2);

    return (void *)result;
}

struct bu_ptbl *
diff_dbip(struct db_i *dbip1, struct db_i *dbip2)
{
    int i;
    struct directory *dp1, *dp2;
    struct gdiff_result *results = NULL;
    struct bu_ptbl *results_table;
    int diff_count = -1;
    int diff_total = 0;

    /* Get a count of the number of objects in the
     * union of the two candidate databases */
    {
	int dbip1_only = 0;
	int dbip2_only = 0;
	int dbip1_dbip2 = 0;

	FOR_ALL_DIRECTORY_START(dp1, dbip1) {
	    /* check if this object exists in the other database */
	    if (db_lookup(dbip2, dp1->d_namep, 0) == RT_DIR_NULL) {
		dbip1_only++;
	    } else {
		dbip1_dbip2++;
	    }
	} FOR_ALL_DIRECTORY_END;
	FOR_ALL_DIRECTORY_START(dp2, dbip2) {
	    /* check if this object exists in the other database */
	    if (db_lookup(dbip1, dp2->d_namep, 0) == RT_DIR_NULL) {
		dbip2_only++;
	    } else {
		dbip1_dbip2++;
	    }
	} FOR_ALL_DIRECTORY_END;
	dbip1_dbip2 = dbip1_dbip2/2;
	diff_total = dbip1_dbip2 + dbip1_only + dbip2_only;
    }

    /* Populate the results array with enough containers to
     * hold the results of processing the two databases */

    results = (struct gdiff_result *)bu_calloc(diff_total + 1, sizeof(struct gdiff_result), "gdiff results array");

    for (i = 0; i < diff_total; i++) {
	gdiff_init(&(results[i]));
    }

    BU_ALLOC(results_table, struct bu_ptbl);
    BU_PTBL_INIT(results_table);

    /* look at all objects in this database */
    FOR_ALL_DIRECTORY_START(dp1, dbip1) {
	struct gdiff_result *curr_result = NULL;
	diff_count++;
	curr_result = &results[diff_count];
	curr_result->dp_orig = dp1;

	/* skip the _GLOBAL object for now - need to deal with this, however */
	if (dp1->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	    continue;
	}
	/* check if this object exists in the other database */
	if ((dp2 = db_lookup(dbip2, dp1->d_namep, 0)) == RT_DIR_NULL) {
	    /*kill_obj(dp1->d_namep);*/
	    curr_result->dp_new = NULL;
	    curr_result->diff_type = 1;
	    continue;
	}
	curr_result->dp_new = dp2;

	(void)diff_dp(curr_result, dp1, dp2, dbip1, dbip2);

    } FOR_ALL_DIRECTORY_END;

    /* now look for objects in the other database that aren't here */
    FOR_ALL_DIRECTORY_START(dp2, dbip2) {
	struct bu_vls s2_tcl = BU_VLS_INIT_ZERO;
	struct bu_attribute_value_set avs2;
	struct gdiff_result *curr_result = NULL;
	BU_AVS_INIT(&avs2);
	/* check if this object exists in the other database */
	if (db_lookup(dbip1, dp2->d_namep, 0) == RT_DIR_NULL) {
	    diff_count++;
	    curr_result = &results[diff_count];
	    curr_result->dp_new = dp2;
	    curr_result->diff_type = 2;
	    if (rt_db_get_internal(curr_result->intern_new, dp2, dbip2, (fastf_t *)NULL, &rt_uniresource) < 0) {
		bu_log("rt_db_get_internal(%s) failure\n", dp2->d_namep);
		curr_result->status = 1;
		continue;
	    }
	    if (curr_result->intern_new->idb_meth->ft_get(&s2_tcl, curr_result->intern_new, NULL) != BRLCAD_ERROR) {
		if (!tcl_list_to_avs(bu_vls_addr(&s2_tcl), &avs2, 1)){
		    curr_result->diff_type = 2;
		    bu_avs_merge(&curr_result->internal_new_only, &avs2);
		}
	    }
	    if (curr_result->intern_new->idb_avs.magic == BU_AVS_MAGIC) {
		bu_avs_merge(&curr_result->additional_new_only, &curr_result->intern_new->idb_avs);
	    }
	}
	bu_avs_free(&avs2);
    } FOR_ALL_DIRECTORY_END;

    for (i = 0; i < diff_total; i++) {
	bu_ptbl_ins(results_table, (long *)&(results[i]));
    }

    return results_table;
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
