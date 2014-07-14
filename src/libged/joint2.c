/*                        J O I N T 2 . C
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
/** @file libged/joint2.c
 *
 * The joint2 command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bio.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"

#include "./ged_private.h"

static struct _ged_funtab joint_subcommand_table[] = {
    {"joint ", "",                        "Joint command table",                                NULL, 0, 0,                     FALSE},
    {"?",      "[commands]",              "summary of available joint commands",                NULL, 0, _GED_FUNTAB_UNLIMITED, FALSE},
    {"accept", "[joints]",                "accept a series of moves",                           NULL, 1, _GED_FUNTAB_UNLIMITED, FALSE},
    {"debug",  "[hex code]",              "Show/set debugging bit vector for joints",           NULL, 1, 2,                     FALSE},
    {"help",   "[commands]",              "give usage message for given joint commands",        NULL, 0, _GED_FUNTAB_UNLIMITED, FALSE},
    {"holds",  "[names]",                 "list constraints",                                   NULL, 1, _GED_FUNTAB_UNLIMITED, FALSE},
    {"list",   "[names]",                 "list joints.",                                       NULL, 1, _GED_FUNTAB_UNLIMITED, FALSE},
    {"load",   "file_name",               "load a joint/constraint file",                       NULL, 2, _GED_FUNTAB_UNLIMITED, FALSE},
    {"mesh",   "",                        "Build the grip mesh",                                NULL, 0, 1,                     FALSE},
    {"move",   "joint_name p1 [p2...p6]", "Manual adjust a joint",                              NULL, 3, 8,                     FALSE},
    {"reject", "[joint_names]",           "reject joint motions",                               NULL, 1, _GED_FUNTAB_UNLIMITED, FALSE},
    {"save",   "file_name",               "Save joints and constraints to disk",                NULL, 2, 2,                     FALSE},
    {"solve",  "constraint",              "Solve a or all constraints",                         NULL, 1, _GED_FUNTAB_UNLIMITED, FALSE},
    {"unload", "",                        "Unload any joint/constraints that have been loaded", NULL, 1, 1,                     FALSE},
    {NULL,     NULL,                      NULL,                                                 NULL, 0, 0,                     FALSE}
};

int
ged_joint2(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *ndp;
    struct rt_db_internal intern;
    /*struct rt_joint_internal *ji; */

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc < 2) {
	struct _ged_funtab *ftp;
	bu_vls_printf(gedp->ged_result_str, "joint <obj> [subcommand]\n");
	bu_vls_printf(gedp->ged_result_str, "The following subcommands are available:\n");
	for (ftp = joint_subcommand_table+1; ftp->ft_name; ftp++) {
	    vls_col_item(gedp->ged_result_str, ftp->ft_name);
	}
	vls_col_eol(gedp->ged_result_str);
	bu_vls_printf(gedp->ged_result_str,"\n");
	return GED_HELP;
    }

    if ((ndp = db_lookup(gedp->ged_wdbp->dbip,  argv[1], LOOKUP_NOISY)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Error: %s is not a solid or does not exist in database", argv[1]);
	return GED_ERROR;
    } 

    GED_DB_GET_INTERNAL(gedp, &intern, ndp, bn_mat_identity, &rt_uniresource, GED_ERROR);


    RT_CK_DB_INTERNAL(&intern);
    /*
    ji = (struct rt_joint_internal*)intern.idb_ptr;
 
    if (ji->magic != RT_JOINT_INTERNAL_MAGIC) {
	bu_vls_printf(gedp->ged_result_str, "Error: %s is not a joint primitive.", ndp->d_namep);
	return GED_ERROR;
    }
    */ 
   
    rt_db_free_internal(&intern);

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
