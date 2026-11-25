/*                         M O V E R . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2026 United States Government as represented by
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
/** @file mged/mover.c
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"

#include "./mged.h"

/*
 * This routine is used when the object to be moved is
 * the top level in its reference path.
 * The object itself (solid or "leaf" combination) is relocated.
 */
int
moveHobj(struct ged *gedp, struct directory *dp, matp_t xlate)
{
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, gedp->dbip, xlate) < 0) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "Could not read the transformed object %s", dp->d_namep);
	return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(dp, gedp->dbip, &intern) < 0) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "Could not write the transformed object %s", dp->d_namep);
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}


/*
 * This routine is used when an instance of an object is to be
 * moved relative to a combination, as opposed to modifying the
 * coordinates of member solids.  Input is a pointer to a COMB,
 * a pointer to an object within the COMB, and modifications.
 */
int
moveHinstance(struct ged *gedp, struct directory *cdp, struct directory *dp, matp_t xlate)
{
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, cdp, gedp->dbip, NULL) < 0 || intern.idb_type != ID_COMBINATION) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "Could not read the parent combination %s", cdp->d_namep);
	return BRLCAD_ERROR;
    }

    struct rt_comb_internal *comb = (struct rt_comb_internal *)intern.idb_ptr;
    union tree *leaf = comb->tree ? db_find_named_leaf(comb->tree, dp->d_namep) : TREE_NULL;
    if (!leaf) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "Could not find the instance %s/%s", cdp->d_namep, dp->d_namep);
	return BRLCAD_ERROR;
    }
    if (leaf->tr_l.tl_mat) {
	bn_mat_mul2(xlate, leaf->tr_l.tl_mat);
    } else {
	leaf->tr_l.tl_mat = (matp_t)bu_malloc(sizeof(mat_t), "instance matrix");
	MAT_COPY(leaf->tr_l.tl_mat, xlate);
    }
    if (rt_db_put_internal(cdp, gedp->dbip, &intern) < 0) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "Could not write the parent combination %s", cdp->d_namep);
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
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
