/*                     D B 5 _ S I Z E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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
/** @addtogroup db5 */
/** @{ */
/** @file librt/db5_size.c
 *
 * Calculate sizes of v5 database objects.
 *
 */

#include "common.h"

#include <set>

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "bnetwork.h"

#include "vmath.h"
#include "bu/avs.h"
#include "rt/db5.h"
#include "raytrace.h"
#include "librt_private.h"

/* TODO - should halt on recursive path, but need full path aware
 * tree walk ala search for that... */
#define DB_SIZE_CYCLIC_LIMIT 10000

HIDDEN int
_bu_avs_size(struct bu_attribute_value_set *avs)
{
    int size = 0;
    struct bu_attribute_value_pair *avpp;
    for (BU_AVS_FOR(avpp, avs)) {
	size += (strlen(avpp->name)+1) * sizeof(char);
	size += (strlen(avpp->value)+1) * sizeof(char);
    }
    return size;
}


HIDDEN void
_db5_calc_subsize(int *ret, union tree *tp, struct db_i *dbip, int *depth, int max_depth, int flags, std::set<struct directory *> &visited,
	void (*traverse_func) (int *ret, struct directory *, struct db_i *, int *, int, int, std::set<struct directory *> &))
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
	    _db5_calc_subsize(ret, tp->tr_b.tb_right, dbip, depth, flags, max_depth, visited, traverse_func);
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    _db5_calc_subsize(ret, tp->tr_b.tb_left, dbip, depth, flags, max_depth, visited, traverse_func);
	    (*depth)--;
	    break;
	case OP_DB_LEAF:
	    if ((dp=db_lookup(dbip, tp->tr_l.tl_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
		(*depth)--;
		return;
	    } else {
		if (!(dp->d_flags & RT_DIR_HIDDEN)) {
		    traverse_func(ret, dp, dbip, depth, flags, max_depth, visited);
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
_db5_calc_size(int *ret, struct directory *dp, struct db_i *dbip, int *depth, int flags, int max_depth, std::set<struct directory *> &visited)
{
    int xpush = flags & DB_SIZE_XPUSH;

    /* If we don't have a variety of things, we're done*/
    if (!dp || !ret || !dbip || !depth) return;

    /* If we're either a) counting all objects regardless of previous visitation status or
     * b) we haven't seen this particular object before, process it */
    if (xpush || visited.find(dp) == visited.end()) {
	/* If we aren't treating things as xpushed, make a note of the current dp */
	if (!xpush) visited.insert(dp);

	/* If we have a comb and we are tree processing, we have to walk the comb */
	if (dp->d_flags & RT_DIR_COMB && max_depth != 0) {
	    /* If we have a comb, open it up. */
	    struct rt_db_internal in;
	    struct rt_comb_internal *comb;
	    if (flags & DB_SIZE_GEOM) {
		(*ret) += dp->d_len;
	    }
	    if (flags & DB_SIZE_ATTR) {
		struct bu_attribute_value_set avs;
		bu_avs_init_empty(&avs);
		db5_get_attributes(dbip, &avs, dp);
		(*ret) += _bu_avs_size(&avs);
	    }

	    if (rt_db_get_internal(&in, dp, dbip, NULL, &rt_uniresource) < 0) return;

	    comb = (struct rt_comb_internal *)in.idb_ptr;
	    _db5_calc_subsize(ret, comb->tree, dbip, depth, flags, max_depth, visited, _db5_calc_size);
	    rt_db_free_internal(&in);
	} else {
	    /* Solid - just increment with its size */
	    if (flags & DB_SIZE_GEOM) {
		(*ret) += dp->d_len;
	    }
	    if (flags & DB_SIZE_ATTR) {
		struct bu_attribute_value_set avs;
		bu_avs_init_empty(&avs);
		db5_get_attributes(dbip, &avs, dp);
		(*ret) += _bu_avs_size(&avs);
	    }
	}
    }
    return;
}


int
db5_size(struct db_i *dbip, struct directory *dp, int flags)
{
    int full_size = 0;
    int depth = 0;
    int local_flags = flags;
    std::set<struct directory *> visited;
    if (!dp || !dbip) return 0;

    if (!flags || flags == DB_SIZE_TREE) {
	local_flags |= DB_SIZE_GEOM;
	local_flags |= DB_SIZE_ATTR;
    }

    if(flags & DB_SIZE_TREE) {
	_db5_calc_size(&full_size, dp, dbip, &depth, local_flags, DB_SIZE_CYCLIC_LIMIT, visited);
    } else {
	_db5_calc_size(&full_size, dp, dbip, &depth, local_flags, 0, visited);
    }

    return full_size;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
