/*                    C Y C L I C . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2025 United States Government as represented by
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
/** @file cyclic.cpp
 *
 * Low level check to identify cyclic geometry references.
 */

#include "common.h"

#include <set>
#include <map>
#include <string>

#include "./ged_lint.h"

/* Because lint is intended to be a deep inspection of the .g looking for problems,
 * we need to do this check using the low level tree walk rather than operating on
 * a search result set (which has checks to filter out cyclic paths.) */
static void
cyclic_search_subtree(struct db_full_path *path, int curr_bool, union tree *tp,
	void (*traverse_func) (struct db_full_path *path, void *), void *client_data)
{
    struct directory *dp;
    lint_data *cdata = (lint_data *)client_data;
    struct ged *gedp = cdata->gedp;
    int bool_val = curr_bool;

    if (!tp) {
	return;
    }

    RT_CK_FULL_PATH(path);
    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    if (tp->tr_op == OP_UNION) bool_val = 2;
	    if (tp->tr_op == OP_INTERSECT) bool_val = 3;
	    if (tp->tr_op == OP_SUBTRACT) bool_val = 4;
	    cyclic_search_subtree(path, bool_val, tp->tr_b.tb_right, traverse_func, client_data);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    cyclic_search_subtree(path, OP_UNION, tp->tr_b.tb_left, traverse_func, client_data);
	    break;
	case OP_DB_LEAF:
	    dp = db_lookup(gedp->dbip, tp->tr_l.tl_name, LOOKUP_QUIET);
	    if (dp == RT_DIR_NULL) {
		return;
	    } else {
		/* See if we've got a cyclic path */
		db_add_node_to_full_path(path, dp);
		DB_FULL_PATH_SET_CUR_BOOL(path, bool_val);
		if (!db_full_path_cyclic(path, NULL, 0)) {
		    /* Not cyclic - keep going */
		    traverse_func(path, client_data);
		} else {
		    /* Cyclic - report it */
		    char *path_string = db_path_to_string(path);
		    nlohmann::json cyclic_json;
		    cyclic_json["problem_type"] = "cyclic_path";
		    cyclic_json["path"] = path_string;
		    cdata->j.push_back(cyclic_json);
		}
		DB_FULL_PATH_POP(path);
		break;
	    }

	default:
	    bu_log("db_functree_subtree: unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("db_functree_subtree: unrecognized operator\n");
    }
}

static void
cyclic_search(struct db_full_path *fp, void *client_data)
{
    lint_data *cdata = (lint_data *)client_data;
    struct ged *gedp = cdata->gedp;
    struct directory *dp;

    RT_CK_FULL_PATH(fp);

    dp = DB_FULL_PATH_CUR_DIR(fp);

    if (!dp) return;

    /* If we're not looking at a comb object we can't have
     * cyclic paths - else, walk the comb */
    if (dp->d_flags & RT_DIR_COMB) {
	struct rt_db_internal in;
	struct rt_comb_internal *comb;

	if (rt_db_get_internal(&in, dp, gedp->dbip, NULL, &rt_uniresource) < 0) return;

	comb = (struct rt_comb_internal *)in.idb_ptr;
	cyclic_search_subtree(fp, OP_UNION, comb->tree, cyclic_search, client_data);
	rt_db_free_internal(&in);
    }
}

int
_ged_cyclic_check(lint_data *cdata)
{
    int i;
    struct directory *dp;
    struct db_full_path *start_path = NULL;
    int ret = BRLCAD_OK;
    if (!cdata)
	return BRLCAD_ERROR;
    if (cdata->argc && !cdata->dpa)
	return BRLCAD_ERROR;

    BU_GET(start_path, struct db_full_path);
    db_full_path_init(start_path);

    if (cdata->argc) {
	for (i = 0; i < cdata->argc; i++) {
	    db_add_node_to_full_path(start_path, cdata->dpa[i]);
	    cyclic_search(start_path, (void *)cdata);
	    DB_FULL_PATH_POP(start_path);
	}
    } else {
	/* We can't do db_ls to get a set of tops objects here, because a cyclic
	 * path can produce a situation where there are no tops objects and/or
	 * hide the paths we need to report. */
	for (i = 0; i < RT_DBNHASH; i++) {
	    for (dp = cdata->gedp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		db_add_node_to_full_path(start_path, dp);
		cyclic_search(start_path, (void *)cdata);
		DB_FULL_PATH_POP(start_path);
	    }
	}
    }

    db_free_full_path(start_path);
    BU_PUT(start_path, struct db_full_path);
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

