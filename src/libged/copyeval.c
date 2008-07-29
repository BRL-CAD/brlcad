/*                         C O P Y E V A L . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file copyeval.c
 *
 * The copyeval command.
 *
 */

#include "common.h"

#include <string.h>

#include "bio.h"
#include "cmd.h"
#include "ged_private.h"

int
ged_copyeval(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal internal, new_int;
    mat_t start_mat;
    int id;
    int i;
    int endpos;
    struct ged_trace_data gtd;
    static const char *usage = "new_prim path_to_old_prim";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (argc < 3 || 27 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* initialize gtd */
    gtd.gtd_gedp = gedp;
    gtd.gtd_flag = GED_CPEVAL;
    gtd.gtd_prflag = 0;

    /* check if new solid name already exists in description */
    if (db_lookup(gedp->ged_wdbp->dbip, argv[1], LOOKUP_QUIET) != DIR_NULL) {
	bu_vls_printf(&gedp->ged_result_str, "%s: alread exists\n", argv[1]);
	return BRLCAD_ERROR;
    }

    MAT_IDN(start_mat);

    /* build directory pointer array for desired path */
    if (argc == 3 && strchr(argv[2], '/')) {
	char *tok;

	endpos = 0;

	tok = strtok((char *)argv[2], "/");
	while (tok) {
	    if ((gtd.gtd_obj[endpos++] = db_lookup(gedp->ged_wdbp->dbip, tok, LOOKUP_NOISY)) == DIR_NULL)
		return BRLCAD_ERROR;
	    tok = strtok((char *)NULL, "/");
	}
    } else {
	for (i=2; i<argc; i++) {
	    if ((gtd.gtd_obj[i-2] = db_lookup(gedp->ged_wdbp->dbip, argv[i], LOOKUP_NOISY)) == DIR_NULL)
		return BRLCAD_ERROR;
	}
	endpos = argc - 2;
    }

    gtd.gtd_objpos = endpos - 1;

    /* Make sure that final component in path is a solid */
    if ((id = rt_db_get_internal(&internal, gtd.gtd_obj[endpos - 1], gedp->ged_wdbp->dbip, bn_mat_identity, &rt_uniresource)) < 0) {
	bu_vls_printf(&gedp->ged_result_str, "import failure on %s\n", argv[argc-1]);
	return BRLCAD_ERROR;
    }

    if (id >= ID_COMBINATION) {
	rt_db_free_internal(&internal, &rt_uniresource);
	bu_vls_printf(&gedp->ged_result_str, "final component on path must be a primitive!!!\n");
	return BRLCAD_ERROR;
    }

    ged_trace(gtd.gtd_obj[0], 0, start_mat, &gtd);

    if (gtd.gtd_prflag == 0) {
	bu_vls_printf(&gedp->ged_result_str, "PATH:  ");

	for (i=0; i<gtd.gtd_objpos; i++)
	    bu_vls_printf(&gedp->ged_result_str, "/%s", gtd.gtd_obj[i]->d_namep);

	bu_vls_printf(&gedp->ged_result_str, "  NOT FOUND\n");
	rt_db_free_internal(&internal, &rt_uniresource);
	return BRLCAD_ERROR;
    }

    /* Have found the desired path - wdb_xform is the transformation matrix */
    /* wdb_xform matrix calculated in wdb_trace() */

    /* create the new solid */
    RT_INIT_DB_INTERNAL(&new_int);
    if (rt_generic_xform(&new_int, gtd.gtd_xform,
			 &internal, 0, gedp->ged_wdbp->dbip, &rt_uniresource)) {
	rt_db_free_internal(&internal, &rt_uniresource);
	bu_vls_printf(&gedp->ged_result_str, "ged_copyeval: rt_generic_xform failed\n");
	return BRLCAD_ERROR;
    }

    if ((dp=db_diradd(gedp->ged_wdbp->dbip, argv[1], -1L, 0,
		      gtd.gtd_obj[endpos-1]->d_flags,
		      (genptr_t)&new_int.idb_type)) == DIR_NULL) {
	rt_db_free_internal(&internal, &rt_uniresource);
	rt_db_free_internal(&new_int, &rt_uniresource);
	bu_vls_printf(&gedp->ged_result_str, "An error has occured while adding a new object to the database.");
	return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &new_int, &rt_uniresource) < 0) {
	rt_db_free_internal(&internal, &rt_uniresource);
	rt_db_free_internal(&new_int, &rt_uniresource);
	bu_vls_printf(&gedp->ged_result_str, "Database write error, aborting");
	return BRLCAD_ERROR;
    }
    rt_db_free_internal(&internal, &rt_uniresource);
    rt_db_free_internal(&new_int, &rt_uniresource);

    return BRLCAD_OK;
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
