/*                        C Y C L I C . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @addtogroup db_search
 * @brief
 * Functionality for locating cyclic paths
 *
 * Cyclic paths (combs whose trees include a reference to the root object
 * within the tree) represent a problem for .g processing logic. (Infinite
 * looping bugs and empty tops lists in particular.)  The routines in
 * this file are designed to identify and list comb objects whose trees
 * contain self referenceing paths - with that information, callers can
 * take steps to either handle or resolve the problems.
 */

#include "common.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <limits.h> /* for INT_MAX */

#include "rt/defines.h"
#include "rt/db_internal.h"
#include "rt/db_io.h"
#include "rt/directory.h"
#include "rt/nongeom.h"
#include "rt/search.h"

/* Search client data container */
struct cyclic_client_data_t {
    struct db_i *dbip;
    struct bu_ptbl *cyclic;
    int cnt;
    int full_search;
};


/**
 * A generic traversal function maintaining awareness of the full path
 * to a given object.
 */
static void
db_fullpath_cyclic_subtree(struct db_full_path *path, int curr_bool, union tree *tp,
			 void (*traverse_func) (struct db_full_path *path, void *),
			 void *client_data)
{
    struct directory *dp;
    struct cyclic_client_data_t *ccd = (struct cyclic_client_data_t *)client_data;
    int bool_val = curr_bool;

    if (!tp)
	return;

    RT_CK_FULL_PATH(path);
    RT_CHECK_DBI(ccd->dbip);
    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    if (tp->tr_op == OP_UNION)
		bool_val = 2;
	    if (tp->tr_op == OP_INTERSECT)
		bool_val = 3;
	    if (tp->tr_op == OP_SUBTRACT)
		bool_val = 4;
	    db_fullpath_cyclic_subtree(path, bool_val, tp->tr_b.tb_right, traverse_func, client_data);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    db_fullpath_cyclic_subtree(path, OP_UNION, tp->tr_b.tb_left, traverse_func, client_data);
	    break;
	case OP_DB_LEAF:
	    if ((dp=db_lookup(ccd->dbip, tp->tr_l.tl_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
		return;
	    } else {
		/* Create the new path. If doing so creates a cyclic path,
		 * abort walk and (if path is minimally cyclic) report it -
		 * else, keep going. */
		struct db_full_path *newpath;
		db_add_node_to_full_path(path, dp);
		if (!db_full_path_cyclic(path, NULL, 0)) {
		    /* Keep going */
		    traverse_func(path, client_data);
		} else {
		    if (ccd->full_search) {
			if (dp == path->fp_names[0]) {
			    // If dp matches the root dp, the cyclic path is a
			    // "minimal" cyclic path.  Otherwise, it is a "buried"
			    // cyclic path - i.e., a reference by a higher level
			    // comb to a comb with a cyclic path problem. A buried
			    // cycle haults the tree walk, but it's not the minimal
			    // "here's the problem to fix" path we want to report
			    // in this case.  (Because we are doing a full database
			    // search, we have to walk all comb trees anyway to be
			    // sure of finding all problems, so we will eventually
			    // find the minimal verison of the problem.)
			    ccd->cnt++;
			    if (ccd->cyclic) {
				BU_ALLOC(newpath, struct db_full_path);
				db_full_path_init(newpath);
				db_dup_full_path(newpath, path);
				/* Insert the path in the bu_ptbl collecting paths */
				bu_ptbl_ins(ccd->cyclic, (long *)newpath);
			    }

			    // Debugging
			    char *path_string = db_path_to_string(path);
			    bu_log("Found minimal cyclic path %s\n", path_string);
			    bu_free(path_string, "free path str");
			}
		    } else {
			// We're not doing a full database search and don't
			// have any guarantees we will find minimal paths at
			// some point during the search; report everything we
			// find within this tree.
			ccd->cnt++;
			if (ccd->cyclic) {
			    BU_ALLOC(newpath, struct db_full_path);
			    db_full_path_init(newpath);
			    db_dup_full_path(newpath, path);
			    /* Insert the path in the bu_ptbl collecting paths */
			    bu_ptbl_ins(ccd->cyclic, (long *)newpath);
			}
		    }
		}
		DB_FULL_PATH_POP(path);
		break;
	    }

	default:
	    bu_log("db_functree_subtree: unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("db_functree_subtree: unrecognized operator\n");
    }
}


/**
 * This walker builds a list of db_full_path entries corresponding to
 * the contents of the tree under *path.  It does so while assigning
 * the boolean operation associated with each path entry to the
 * db_full_path structure.  This list is then used for further
 * processing and filtering by the search routines.
 */
static void
db_fullpath_cyclic(struct db_full_path *path, void *client_data)
{
    struct directory *dp;
    struct cyclic_client_data_t *ccd= (struct cyclic_client_data_t *)client_data;
    RT_CK_FULL_PATH(path);
    RT_CK_DBI(ccd->dbip);

    dp = DB_FULL_PATH_CUR_DIR(path);
    if (!dp)
	return;
    if (dp->d_flags & RT_DIR_COMB) {
	struct rt_db_internal in;
	struct rt_comb_internal *comb;

	if (rt_db_get_internal(&in, dp, ccd->dbip, NULL, &rt_uniresource) < 0)
	    return;

	comb = (struct rt_comb_internal *)in.idb_ptr;
	db_fullpath_cyclic_subtree(path, OP_UNION, comb->tree, db_fullpath_cyclic, client_data);
	rt_db_free_internal(&in);
    }
}

int
db_cyclic_paths(struct bu_ptbl *cyclic_paths, struct db_i *dbip, struct directory *sdp)
{
    if (!dbip)
	return 0;

    /* First, check if cyclic is initialized - don't trust the caller to do it,
     * but it's fine if they did */
    if (cyclic_paths && cyclic_paths != BU_PTBL_NULL) {
	if (!BU_PTBL_IS_INITIALIZED(cyclic_paths)) {
	    BU_PTBL_INIT(cyclic_paths);
	}
    }

    struct cyclic_client_data_t ccd;
    ccd.dbip = dbip;
    ccd.cyclic = cyclic_paths;
    ccd.cnt = 0;

    if (sdp) {
	if (!(sdp->d_flags & RT_DIR_COMB))
	    return 0;
	ccd.full_search = 0;
	struct db_full_path *start_path = NULL;
	BU_ALLOC(start_path, struct db_full_path);
	db_full_path_init(start_path);
	db_add_node_to_full_path(start_path, sdp);
	db_fullpath_cyclic(start_path, (void **)&ccd);
	db_free_full_path(start_path);
	bu_free(start_path, "start_path");
    } else {
	struct directory *dp;
	ccd.full_search = 1;
	for (int i = 0; i < RT_DBNHASH; i++) {
	    for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		if (!(dp->d_flags & RT_DIR_COMB))
		    continue;

		struct db_full_path *start_path = NULL;
		BU_ALLOC(start_path, struct db_full_path);
		db_full_path_init(start_path);
		db_add_node_to_full_path(start_path, dp);
		db_fullpath_cyclic(start_path, (void **)&ccd);
		db_free_full_path(start_path);
		bu_free(start_path, "start_path");
	    }
	}
    }

    return ccd.cnt;
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
