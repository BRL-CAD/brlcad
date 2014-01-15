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
#include "bio.h"

#include "tcl.h"

#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "db_diff.h"


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
	    if (del_func(dbip1, dbip2, dp1, client_data)) error--;
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
	    if (chgd_func(dbip1, dbip2, dp1, dp2, client_data)) error--;
	} else {
	    if (unch_func(dbip1, dbip2, dp1, client_data)) error--;
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
	    if (add_func(dbip1, dbip2, dp1, client_data)) error--;
	}

	ret += this_diff;

    } FOR_ALL_DIRECTORY_END;

    if (!error) {
    	return ret;
    } else {
	return error;
    }
}


HIDDEN int
avpp_val_compare(const char *val1, const char *val2)
{
    return BU_STR_EQUAL(val1, val2);
}

/* TODO - is this appropriate for libbu? */
HIDDEN int
bu_avs_diff(struct bu_attribute_value_set *shared,
	struct bu_attribute_value_set *orig_only,
	struct bu_attribute_value_set *new_only,
	struct bu_attribute_value_set *orig_diff,
	struct bu_attribute_value_set *new_diff,
	const struct bu_attribute_value_set *avs1,
	const struct bu_attribute_value_set *avs2)
{
    int have_diff = 0;
    struct bu_attribute_value_pair *avp;
    if (!BU_AVS_IS_INITIALIZED(shared)) BU_AVS_INIT(shared);
    if (!BU_AVS_IS_INITIALIZED(orig_only)) BU_AVS_INIT(orig_only);
    if (!BU_AVS_IS_INITIALIZED(new_only)) BU_AVS_INIT(new_only);
    if (!BU_AVS_IS_INITIALIZED(orig_diff)) BU_AVS_INIT(orig_diff);
    if (!BU_AVS_IS_INITIALIZED(new_diff)) BU_AVS_INIT(new_diff);
    for (BU_AVS_FOR(avp, avs1)) {
	const char *val2 = bu_avs_get(avs2, avp->name);
	if (!val2) {
	    bu_avs_add(orig_only, avp->name, avp->value);
	    have_diff++;
	}
	if (avpp_val_compare(avp->value, val2)) {
	    bu_avs_add(shared, avp->name, avp->value);
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
	   struct bu_attribute_value_set *unchanged)
{
    int do_all = 0;
    int has_diff = 0;

    if (!left_obj || !right_obj || !added || !removed || !changed_left || !changed_right || !unchanged) return -1;
    if (!BU_AVS_IS_INITIALIZED(added)) BU_AVS_INIT(added);
    if (!BU_AVS_IS_INITIALIZED(removed)) BU_AVS_INIT(removed);
    if (!BU_AVS_IS_INITIALIZED(changed_left)) BU_AVS_INIT(changed_left);
    if (!BU_AVS_IS_INITIALIZED(changed_right)) BU_AVS_INIT(changed_right);
    if (!BU_AVS_IS_INITIALIZED(unchanged)) BU_AVS_INIT(unchanged);

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
	 * the attribute/value paradigm used for the majority of the comparisions.  For
	 * this reason, we handle it specially using the lower level database time
	 * information directly.
	 */
	if (left_obj->idb_minor_type != right_obj->idb_minor_type) {
	    bu_avs_add(changed_left, "object type", left_obj->idb_meth->ft_label);
	    bu_avs_add(changed_right, "object type", right_obj->idb_meth->ft_label);
	    has_diff += 1;
	} else {
	    if (left_obj->idb_minor_type == DB5_MINORTYPE_BRLCAD_ARB8) {
		struct bn_tol arb_tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1.0 - 1e-6 };
		int arb_type_1 = rt_arb_std_type(left_obj, &arb_tol);
		int arb_type_2 = rt_arb_std_type(right_obj, &arb_tol);
		if (arb_type_1 != arb_type_2) {
		    bu_avs_add(changed_left, "object type", arb_type_to_str(arb_type_1));
		    bu_avs_add(changed_right, "object type", arb_type_to_str(arb_type_2));
		    has_diff += 1;
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
	    has_diff += bu_avs_diff(unchanged, removed, added, changed_left, changed_right, &avs1, &avs2);
	}

	bu_avs_free(&avs1);
	bu_avs_free(&avs2);

    }

    if (flags == DB_COMPARE_ATTRS || do_all) {

	if (left_obj->idb_avs.magic == BU_AVS_MAGIC && right_obj->idb_avs.magic == BU_AVS_MAGIC) {
	    has_diff += bu_avs_diff(unchanged, removed, added, changed_left, changed_right, &left_obj->idb_avs, &right_obj->idb_avs);
	} else {
	    if (left_obj->idb_avs.magic == BU_AVS_MAGIC) {
		bu_avs_merge(removed, &left_obj->idb_avs);
		has_diff++;
	    }
	    if (right_obj->idb_avs.magic == BU_AVS_MAGIC) {
		bu_avs_merge(added, &right_obj->idb_avs);
		has_diff++;
	    }
	}

    }

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
