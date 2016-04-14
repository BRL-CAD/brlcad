/*                         E X I S T S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2016 United States Government as represented by
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
/** @file libged/stat.c
 *
 * Report information about database objects.
 *
 * Modeled on the Unix "stat" command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/opt.h"

#include "./ged_private.h"

/* Based on the f_type logic from search, but we can't make this a common
 * function because we're mallocing strings here and there are a lot of
 * situations in the search logic where we don't need/want the string itself.
 * Still need a better/universal/low-cost way to get a descriptive string for
 * an object's type.
 */
HIDDEN char *
_ged_db_obj_type(struct db_i *dbip, struct directory *dp)
{
    int type;
    char *retstr = NULL;
    struct rt_db_internal intern;
    if (!dp || dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) return NULL;

    if (dp->d_flags & RT_DIR_COMB) {
	if (dp->d_flags & RT_DIR_REGION) {
	    retstr = bu_strdup("region");
	} else {
	    retstr = bu_strdup("comb");
	}
	return retstr;
    }

    if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) return NULL;
    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD) {
	rt_db_free_internal(&intern);
	return NULL;
    }

    switch (intern.idb_minor_type) {
	case DB5_MINORTYPE_BRLCAD_ARB8:
	    type = rt_arb_std_type(&intern, NULL);
	    switch (type) {
		case 4:
		    retstr = bu_strdup("arb4");
		    break;
		case 5:
		    retstr = bu_strdup("arb5");
		    break;
		case 6:
		    retstr = bu_strdup("arb6");
		    break;
		case 7:
		    retstr = bu_strdup("arb7");
		    break;
		case 8:
		    retstr = bu_strdup("arb8");
		    break;
		default:
		    retstr = NULL;
		    break;
	    }
	    break;
	case DB5_MINORTYPE_BRLCAD_METABALL:
	    /* Because ft_label is only 8 characters, ft_label doesn't work in fnmatch for metaball*/
	    retstr = bu_strdup("metaball");
	    break;
	default:
	    retstr = bu_strdup(intern.idb_meth->ft_label);
	    break;
    }

    rt_db_free_internal(&intern);
    return retstr;
}

HIDDEN void
calc_subsize(int *ret, union tree *tp, struct db_i *dbip, int *depth, int max_depth,
	void (*traverse_func) (int *ret, struct directory *, struct db_i *, int *, int))
{
    struct directory *dp;
    if (!tp) return;
    RT_CHECK_DBI(dbip);
    RT_CK_TREE(tp);
    (*depth)++;
    if (*depth > max_depth) {
	(*depth)--;
	return;
    }
    switch (tp->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    calc_subsize(ret, tp->tr_b.tb_right, dbip, depth, max_depth, traverse_func);
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    calc_subsize(ret, tp->tr_b.tb_left, dbip, depth, max_depth, traverse_func);
	    (*depth)--;
	    break;
	case OP_DB_LEAF:
	    if ((dp=db_lookup(dbip, tp->tr_l.tl_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
		(*depth)--;
		return;
	    } else {
		if (!(dp->d_flags & RT_DIR_HIDDEN)) {
		    traverse_func(ret, dp, dbip, depth, max_depth);
		}
		(*depth)--;
		break;
	    }

	default:
	    bu_log("db_functree_subtree: unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("db_functree_subtree: unrecognized operator\n");
    }
    return;
}

HIDDEN void
calc_size(int *ret, struct directory *dp, struct db_i *dbip, int *depth, int max_depth)
{

    /* If we don't have a variety of things, we're done*/
    if (!dp || !ret || !dbip || !depth) return;

    if (dp->d_flags & RT_DIR_COMB) {
	/* If we have a comb, open it up. */
	struct rt_db_internal in;
	struct rt_comb_internal *comb;
	(*ret) += dp->d_len;

	if (rt_db_get_internal(&in, dp, dbip, NULL, &rt_uniresource) < 0) return;

	comb = (struct rt_comb_internal *)in.idb_ptr;
	calc_subsize(ret, comb->tree, dbip, depth, max_depth, calc_size);
	rt_db_free_internal(&in);
    } else {
	/* Solid - just increment with its size */
	(*ret) += dp->d_len;
    }
    return;
}

#define CYCLIC_LIMIT 10000
/**
 * Checks for the existence of a specified object.
 */
int
ged_stat(struct ged *gedp, int argc, const char **argv)
{
    char *type = NULL;
    int full_size = 0;
    int depth = 0;
    /* The directory struct is the closest analog to a filesystem's
     * stat structure.  Don't currently have things like create/access/modify
     * time stamps recorded in that struct, or for that matter in the
     * database, but if/when we decide to do so follow the UNIX stat
     * command model to report/expose that info here... */
    struct directory *dp = RT_DIR_NULL;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc < 2) return GED_ERROR;

    dp = db_lookup(gedp->ged_wdbp->dbip, argv[1], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Cannot stat %s: no such object.", argv[1]);
	return GED_ERROR;
    }

    type = _ged_db_obj_type(gedp->ged_wdbp->dbip, dp);

    calc_size(&full_size, dp, gedp->ged_wdbp->dbip, &depth, 10000);
    if (depth >= CYCLIC_LIMIT) {
	bu_vls_printf(gedp->ged_result_str, "Error: depth greater than %d encountered, possible cyclic hierarchy.\n", CYCLIC_LIMIT);
	return GED_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str, "Object: %s  Type: %s\n", dp->d_namep, type);
    bu_vls_printf(gedp->ged_result_str, "Size(obj): %d Size(full): %d\n", dp->d_len, full_size, type);

    bu_free(type, "free type string");
    return GED_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
