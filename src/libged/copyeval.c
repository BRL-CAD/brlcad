/*                         C O P Y E V A L . C
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
/** @file libged/copyeval.c
 *
 * The copyeval command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


int
ged_copyeval(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal internal, new_int;
    struct rt_db_internal *ip;
    mat_t start_mat;
    int i;
    int endpos;
    char *tok;
    struct _ged_trace_data gtd;
    static const char *usage = "path_to_old_prim new_prim";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* initialize gtd */
    gtd.gtd_gedp = gedp;
    gtd.gtd_flag = _GED_CPEVAL;
    gtd.gtd_prflag = 0;

    /* check if new solid name already exists in description */
    GED_CHECK_EXISTS(gedp, argv[2], LOOKUP_QUIET, GED_ERROR);

    MAT_IDN(start_mat);

    /* build directory pointer array for desired path */
    if (strchr(argv[1], '/')) {
	endpos = 0;

	tok = strtok((char *)argv[1], "/");
	while (tok) {
	    GED_DB_LOOKUP(gedp, gtd.gtd_obj[endpos], tok, LOOKUP_NOISY, GED_ERROR & GED_QUIET);
	    endpos++;
	    tok = strtok((char *)NULL, "/");
	}
    } else {
	endpos = 0;
	GED_DB_LOOKUP(gedp, gtd.gtd_obj[endpos], argv[1], LOOKUP_NOISY, GED_ERROR & GED_QUIET);
	endpos++;
    }

    gtd.gtd_objpos = endpos - 1;

    GED_DB_GET_INTERNAL(gedp, &internal, gtd.gtd_obj[endpos - 1], bn_mat_identity, &rt_uniresource, GED_ERROR);

    if (endpos > 1) {
	/* Make sure that final component in path is a solid */
	if (internal.idb_type == ID_COMBINATION) {
	    rt_db_free_internal(&internal);
	    bu_vls_printf(gedp->ged_result_str, "final component on path must be a primitive!\n");
	    return GED_ERROR;
	}

	/* Accumulate the matrices */
	_ged_trace(gtd.gtd_obj[0], 0, start_mat, &gtd);

	if (gtd.gtd_prflag == 0) {
	    bu_vls_printf(gedp->ged_result_str, "PATH:  ");

	    for (i=0; i<gtd.gtd_objpos; i++)
		bu_vls_printf(gedp->ged_result_str, "/%s", gtd.gtd_obj[i]->d_namep);

	    bu_vls_printf(gedp->ged_result_str, "  NOT FOUND\n");
	    rt_db_free_internal(&internal);
	    return GED_ERROR;
	}

	/* Have found the desired path - wdb_xform is the transformation matrix */
	/* wdb_xform matrix calculated in wdb_trace() */

	/* create the new solid */
	RT_DB_INTERNAL_INIT(&new_int);
	if (rt_generic_xform(&new_int, gtd.gtd_xform,
			     &internal, 0, gedp->ged_wdbp->dbip, &rt_uniresource)) {
	    rt_db_free_internal(&internal);
	    bu_vls_printf(gedp->ged_result_str, "ged_copyeval: rt_generic_xform failed\n");
	    return GED_ERROR;
	}

	ip = &new_int;
    } else
	ip = &internal;

    /* should call GED_DB_DIRADD() but need to deal with freeing the
     * internals on failure.
     */
    dp=db_diradd(gedp->ged_wdbp->dbip, argv[2], RT_DIR_PHONY_ADDR, 0, gtd.gtd_obj[endpos-1]->d_flags, (genptr_t)&ip->idb_type);
    if (dp == RT_DIR_NULL) {
	rt_db_free_internal(&internal);
	if (ip == &new_int)
	    rt_db_free_internal(&new_int);
	bu_vls_printf(gedp->ged_result_str, "An error has occured while adding a new object to the database.");
	return GED_ERROR;
    }

    /* should call GED_DB_DIRADD() but need to deal with freeing the
     * internals on failure.
     */
    if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, ip, &rt_uniresource) < 0) {
	/* if (ip == &new_int) then new_int gets freed by the rt_db_put_internal above
	 * regardless of whether it succeeds or not. At this point only internal needs
	 * to be freed. On the other hand if (ip == &internal), the internal gets freed
	 * freed by the rt_db_put_internal above. In this case memory for new_int has
	 * not been allocated.
	 */
	if (ip == &new_int)
	    rt_db_free_internal(&internal);

	bu_vls_printf(gedp->ged_result_str, "Database write error, aborting");
	return GED_ERROR;
    }

    /* see previous comment */
    if (ip == &new_int)
	rt_db_free_internal(&internal);

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
