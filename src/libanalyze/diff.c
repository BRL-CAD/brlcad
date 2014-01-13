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
 * Internal diff types:
 *
 * 0 - no difference;
 * 1 - in original only;
 * 2 - in new only;
 * 3 - type difference;
 * 4 - binary-only difference;
 * 5 - parameter differences;
 *
 * Additional diff types (valid only for internal diff types 0, 3, 4, and 5):
 *  0 - no difference;
 *  1 - in original only;
 *  2 - in new only;
 *  3 - additional attributes in original only, no common with differing values;
 *  4 - additional attributes in new only, no common with differing values;
 *  5 - complex attribute diffs
 */
struct gdiff_result {
    uint32_t diff_magic;
    int status;
    struct directory *dp_orig;
    struct directory *dp_new;
    struct rt_db_internal *intern_orig;
    struct rt_db_internal *intern_new;
    int internal_diff_type;
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

void
gdiff_init(struct gdiff_result *result)
{
    result->diff_magic = GDIFF_MAGIC;
    result->status = 0;
    result->internal_diff_type = 0;
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
gdiff_free(struct gdiff_result *result)
{
    rt_db_free_internal(result->intern_orig);
    rt_db_free_internal(result->intern_new);
    bu_avs_free(&result->internal_shared);
    bu_avs_free(&result->internal_orig_only);
    bu_avs_free(&result->internal_new_only);
    bu_avs_free(&result->internal_orig_diff);
    bu_avs_free(&result->internal_new_diff);
    bu_avs_free(&result->additional_shared);
    bu_avs_free(&result->additional_orig_only);
    bu_avs_free(&result->additional_new_only);
    bu_avs_free(&result->additional_orig_diff);
    bu_avs_free(&result->additional_new_diff);
}

void
gdiff_print(struct gdiff_result *result)
{
    struct bu_vls tmp = BU_VLS_INIT_ZERO;
    struct directory *dp = result->dp_orig;
    if (result->internal_diff_type == 2) {
	dp = result->dp_new;
    }
    bu_log("\n\n%s(%d): (internal type: %d)\n", dp->d_namep, result->status, result->internal_diff_type);
    bu_vls_sprintf(&tmp, "Internal parameters: shared (%s)", dp->d_namep);
    bu_avs_print(&result->internal_shared, bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Internal parameters: orig_only (%s)", dp->d_namep);
    bu_avs_print(&result->internal_orig_only, bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Internal parameters: new_only (%s)", dp->d_namep);
    bu_avs_print(&result->internal_new_only, bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Internal parameters: orig_diff (%s)", dp->d_namep);
    bu_avs_print(&result->internal_orig_diff, bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Internal parameters: new_diff (%s)", dp->d_namep);
    bu_avs_print(&result->internal_new_diff, bu_vls_addr(&tmp));

    bu_vls_sprintf(&tmp, "Additional parameters: shared (%s)", dp->d_namep);
    bu_avs_print(&result->additional_shared, bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Additional parameters: orig_only (%s)", dp->d_namep);
    bu_avs_print(&result->additional_orig_only, bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Additional parameters: new_only (%s)", dp->d_namep);
    bu_avs_print(&result->additional_new_only, bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Additional parameters: orig_diff (%s)", dp->d_namep);
    bu_avs_print(&result->additional_orig_diff, bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Additional parameters: new_diff (%s)", dp->d_namep);
    bu_avs_print(&result->additional_new_diff, bu_vls_addr(&tmp));
}

void
attrs_summary(struct bu_vls *attr_log, struct gdiff_result *result)
{
    struct bu_attribute_value_pair *avpp;
    if (result->additional_orig_only.count > 0) {
	bu_vls_printf(attr_log, "   Attributes removed:\n");
	for (BU_AVS_FOR(avpp, &(result->additional_orig_only))) {
	    bu_vls_printf(attr_log, "      %s\n", avpp->name);
	}
    }
    if (result->additional_new_only.count > 0) {
	bu_vls_printf(attr_log, "   Attributes added:\n");
	for (BU_AVS_FOR(avpp, &(result->additional_new_only))) {
	    bu_vls_printf(attr_log, "      %s: %s\n", avpp->name, avpp->value);
	}
    }
    if (result->additional_orig_diff.count > 0) {
	bu_vls_printf(attr_log, "   Attribute parameters changed:\n");
	for (BU_AVS_FOR(avpp, &(result->internal_orig_diff))) {
	    bu_vls_printf(attr_log, "      %s: \"%s\" -> \"%s\"\n", avpp->name, avpp->value, bu_avs_get(&(result->additional_new_diff), avpp->name));
	}
    }
}

void
gdiff_summary(int result_count, struct gdiff_result *results)
{
    int i = 0;
    struct bu_vls attr = BU_VLS_INIT_ZERO;
    struct bu_vls params = BU_VLS_INIT_ZERO;
    struct bu_vls added = BU_VLS_INIT_ZERO;
    struct bu_vls removed = BU_VLS_INIT_ZERO;
    struct bu_vls typechanged = BU_VLS_INIT_ZERO;
    struct bu_vls same = BU_VLS_INIT_ZERO;
    struct bu_attribute_value_pair *avpp;
    for (i = 0; i < result_count; i++) {
	struct gdiff_result *result = &(results[i]);
	struct directory *dp = result->dp_orig;
	if (result->internal_diff_type == 2) {
	    dp = result->dp_new;
	}
	switch (result->internal_diff_type) {
	    case 0:
		bu_vls_trunc(&attr, 0);
		attrs_summary(&attr, result);
		if (strlen(bu_vls_addr(&attr)) > 0)
		    bu_vls_printf(&same, "%s:\n%s\n", dp->d_namep, bu_vls_addr(&attr));
		break;
	    case 1:
		bu_vls_printf(&removed, "%s was removed.\n\n", dp->d_namep);
		break;
	    case 2:
		bu_vls_printf(&added, "%s was added.\n\n", dp->d_namep);
		break;
	    case 3:
		bu_vls_printf(&typechanged, "%s changed from type \"%s\" to type \"%s\".\n", dp->d_namep, result->intern_orig->idb_meth->ft_label, result->intern_new->idb_meth->ft_label);
		attrs_summary(&typechanged, result);
		bu_vls_printf(&typechanged, "\n");
		break;
	    case 4:
		bu_vls_printf(&params, "%s exhibits a binary difference.\n", dp->d_namep);
		attrs_summary(&params, result);
		bu_vls_printf(&params, "\n");
		break;
	    case 5:
		bu_vls_printf(&params, "%s parameters changed:\n", dp->d_namep);
		for (BU_AVS_FOR(avpp, &(result->internal_orig_diff))) {
		    bu_vls_printf(&params, "   %s: \"%s\" -> \"%s\"\n", avpp->name, avpp->value, bu_avs_get(&(result->internal_new_diff), avpp->name));
		}
		attrs_summary(&params, result);
		bu_vls_printf(&params, "\n");
		break;
	    default:
		break;
	}
    }
    if (strlen(bu_vls_addr(&same)) > 0) bu_log("%s", bu_vls_addr(&same));
    if (strlen(bu_vls_addr(&removed)) > 0) bu_log("%s", bu_vls_addr(&removed));
    if (strlen(bu_vls_addr(&added)) > 0) bu_log("%s", bu_vls_addr(&added));
    if (strlen(bu_vls_addr(&typechanged)) > 0) bu_log("%s", bu_vls_addr(&typechanged));
    if (strlen(bu_vls_addr(&params)) > 0) bu_log("%s", bu_vls_addr(&params));
    for (i = 0; i < result_count; i++) {
	/*gdiff_print(&(results[i]));*/
    }
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

int
diff_dp(struct gdiff_result *result, struct directory *dp1, struct directory *dp2,
	struct db_i *dbip1, struct db_i *dbip2)
{
    struct bu_attribute_value_set avs1, avs2;
    struct bu_vls s1_tcl = BU_VLS_INIT_ZERO;
    struct bu_vls s2_tcl = BU_VLS_INIT_ZERO;
    int have_tcl1 = 1;
    int have_tcl2 = 1;

    if (!(GDIFF_IS_INITIALIZED(result))) gdiff_init(result);

    /* Get the internal objects */
    if (rt_db_get_internal(result->intern_orig, dp1, dbip1, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_log("rt_db_get_internal(%s) failure\n", dp1->d_namep);
	result->status = 1;
	return -1;
    }
    if (rt_db_get_internal(result->intern_new, dp2, dbip2, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_log("rt_db_get_internal(%s) failure\n", dp2->d_namep);
	result->status = 1;
	return -1;
    }

    /* Do some type based checking - this will make a difference in
     * subsequent decision trees */
    if (result->intern_orig->idb_minor_type != result->intern_new->idb_minor_type) {
	result->internal_diff_type = 3;
    } else {
	if (result->intern_orig->idb_minor_type == DB5_MINORTYPE_BRLCAD_ARB8) {
	    struct bn_tol arb_tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1.0 - 1e-6 };
	    int arb_type_1 = rt_arb_std_type(result->intern_orig, &arb_tol);
	    int arb_type_2 = rt_arb_std_type(result->intern_new, &arb_tol);
	    if (arb_type_1 != arb_type_2) result->internal_diff_type = 3;
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
	if (avs_diff_result && !result->internal_diff_type) result->internal_diff_type = 5;
    } else {
	/* If we reach this point and don't have successful avs creation, we are reduced
	 * to comparing the binary serializations of the two objects.  This is not ideal
	 * in that it precludes a nuanced description of the differences, but it is at
	 * least able to detect them.  We don't do this comparison for two different
	 * object types, since they are already assumed to be different. */
	if (result->internal_diff_type != 3) {
	    struct bu_external ext1, ext2;

	    if (db_get_external(&ext1, dp1, dbip1)) {
		bu_log("ERROR: db_get_external failed on solid %s in %s\n", dp1->d_namep, dbip1->dbi_filename);
		result->status = 1;
		return -1;
	    }
	    if (db_get_external(&ext2, dp2, dbip2)) {
		bu_log("ERROR: db_get_external failed on solid %s in %s\n", dp2->d_namep, dbip2->dbi_filename);
		result->status = 1;
		return -1;
	    }

	    if (ext1.ext_nbytes != ext2.ext_nbytes) {
		result->internal_diff_type = 4;
	    }

	    if (!(result->internal_diff_type != 4) &&
		    memcmp((void *)ext1.ext_buf, (void *)ext2.ext_buf, ext1.ext_nbytes)) {
		result->internal_diff_type = 4;
	    }
	}
    }

    /* Look at the extra attributes as well */
    if (result->intern_orig->idb_avs.magic == BU_AVS_MAGIC &&
	    result->intern_new->idb_avs.magic == BU_AVS_MAGIC) {
	(void)bu_avs_diff(&result->additional_shared,
		&result->additional_orig_only,
		&result->additional_new_only, &result->additional_orig_diff,
		&result->additional_new_diff,
		&result->intern_orig->idb_avs,
		&result->intern_new->idb_avs,
		(struct bn_tol *)NULL);
    } else {
	if (result->intern_orig->idb_avs.magic == BU_AVS_MAGIC) {
	    bu_avs_merge(&result->additional_orig_only, &result->intern_orig->idb_avs);
	}
	if (result->intern_new->idb_avs.magic == BU_AVS_MAGIC) {
	    bu_avs_merge(&result->additional_new_only, &result->intern_new->idb_avs);
	}
    }

    bu_avs_free(&avs1);
    bu_avs_free(&avs2);

    return 0;
}

int
diff_dbip(struct db_i *dbip1, struct db_i *dbip2)
{
    int i;
    struct directory *dp1, *dp2;
    struct gdiff_result *results = NULL;
    int diff_count = -1;
    int diff_total = 0;
    /*struct bu_vls diff_log = BU_VLS_INIT_ZERO;*/

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
	    curr_result->internal_diff_type = 1;
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
	    curr_result->internal_diff_type = 2;
	    if (rt_db_get_internal(curr_result->intern_new, dp2, dbip2, (fastf_t *)NULL, &rt_uniresource) < 0) {
		bu_log("rt_db_get_internal(%s) failure\n", dp2->d_namep);
		curr_result->status = 1;
		continue;
	    }
	    if (curr_result->intern_new->idb_meth->ft_get(&s2_tcl, curr_result->intern_new, NULL) != BRLCAD_ERROR) {
		if (!tcl_list_to_avs(bu_vls_addr(&s2_tcl), &avs2, 1)){
		    curr_result->internal_diff_type = 2;
		    bu_avs_merge(&curr_result->internal_new_only, &avs2);
		}
	    }
	    if (curr_result->intern_new->idb_avs.magic == BU_AVS_MAGIC) {
		bu_avs_merge(&curr_result->additional_new_only, &curr_result->intern_new->idb_avs);
	    }
	}
	bu_avs_free(&avs2);
    } FOR_ALL_DIRECTORY_END;


    gdiff_summary(diff_total, results);

    for (i = 0; i < diff_total; i++) {
	gdiff_free(&(results[i]));
    }
    return 0;
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
