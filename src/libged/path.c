/*                         P A T H . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/path.c
 *
 *
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "ged.h"


/*
 * A recursive function that is only called by ged_path_validate()
 */
HIDDEN int
path_validate_recurse(struct ged *gedp, struct db_full_path *path,
		      struct directory *roots_child)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct directory *root = DB_FULL_PATH_ROOT_DIR(path);

    /* get comb object */
    if (rt_db_get_internal(&intern, root, gedp->ged_wdbp->dbip,
			   (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	return GED_ERROR;
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;

    /* see if we really have a child of the root dir */
    if (db_find_named_leaf(comb->tree, roots_child->d_namep) != TREE_NULL) {
	rt_db_free_internal(&intern);
	if (!(path->fp_len > 2))
	    return GED_OK; /* no more children */
	else if (roots_child->d_flags & RT_DIR_COMB) {
	    /* remove root dir */
	    ++(path->fp_names);
	    --(path->fp_len);
	    path_validate_recurse(gedp, path, DB_FULL_PATH_GET(path, 1));
	} else
	    return GED_ERROR; /* non-combinations shouldn't have children */
    } else {
	rt_db_free_internal(&intern);
	return GED_ERROR; /* that child doesn't exist under root */
    }
    return GED_OK; /* for compiler */
}


/**
 * Checks that each directory in the supplied path actually has the
 * subdirectories that are implied by the path. Returns GED_OK if
 * true, or GED_ERROR if false.
 */
int
ged_path_validate(struct ged *gedp, const struct db_full_path *const path)
{
    /* Since this is a db_full_path, we already know that each
     * directory exists at root, and just need to check the order */
    struct directory *root;
    struct db_full_path path_tmp;
    int ret;

    db_full_path_init(&path_tmp);
    db_dup_full_path(&path_tmp, path);

    if (path_tmp.fp_len <= 1)
	return GED_OK; /* TRUE; no children */

    root = DB_FULL_PATH_ROOT_DIR(&path_tmp);
    if (!(root->d_flags & RT_DIR_COMB))
	return GED_ERROR; /* has children, but isn't a combination */

    ret = path_validate_recurse(gedp, &path_tmp,
				DB_FULL_PATH_GET(&path_tmp, 1));
    db_free_full_path(&path_tmp);
    return ret;
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
