/*                        J O I N T 2 . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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


#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../ged_private.h"

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
joint_selection(
    struct ged *gedp,
    struct rt_db_internal *ip,
    int argc,
    const char *argv[])
{
    int i;
    struct rt_selection_set *selection_set;
    struct bu_ptbl *selections;
    struct rt_selection *new_selection;
    struct rt_selection_query query;
    const char *cmd, *solid_name, *selection_name;

    /*   0         1            2         3
     * joint2 <solid_name> selection subcommand
     */
    if (argc < 4) {
	bu_vls_printf(gedp->ged_result_str, "not enough args for selection replace\n");
	return GED_ERROR;
    }

    solid_name = argv[1];
    cmd = argv[3];

    if (BU_STR_EQUAL(cmd, "replace")) {
	/* append to named selection - selection is created if it doesn't exist */
	void (*free_selection)(struct rt_selection *);

	/*        4         5      6      7     8    9    10
	 * selection_name startx starty startz dirx diry dirz
	 */
	if (argc != 11) {
	    bu_vls_printf(gedp->ged_result_str, "wrong args for selection replace\n");
	    return GED_ERROR;
	}
	selection_name = argv[4];

	/* find matching selections */
	query.start[X] = atof(argv[5]);
	query.start[Y] = atof(argv[6]);
	query.start[Z] = atof(argv[7]);
	query.dir[X] = atof(argv[8]);
	query.dir[Y] = atof(argv[9]);
	query.dir[Z] = atof(argv[10]);
	query.sorting = RT_SORT_CLOSEST_TO_START;

	selection_set = ip->idb_meth->ft_find_selections(ip, &query);
	if (!selection_set) {
	    bu_vls_printf(gedp->ged_result_str, "no matching selections");
	    return GED_OK;
	}

	/* could be multiple options, just grabbing the first and
	 * freeing the rest
	 */
	selections = &selection_set->selections;
	new_selection = (struct rt_selection *)BU_PTBL_GET(selections, 0);

	free_selection = selection_set->free_selection;
	for (i = BU_PTBL_LEN(selections) - 1; i > 0; --i) {
	    long *s = BU_PTBL_GET(selections, i);
	    free_selection((struct rt_selection *)s);
	    bu_ptbl_rm(selections, s);
	}
	bu_ptbl_free(selections);
	BU_FREE(selection_set, struct rt_selection_set);

	/* get existing/new selections set in gedp */
	selection_set = ged_get_selection_set(gedp, solid_name, selection_name);
	selection_set->free_selection = free_selection;
	selections = &selection_set->selections;

	for (i = BU_PTBL_LEN(selections) - 1; i >= 0; --i) {
	    long *s = BU_PTBL_GET(selections, i);
	    free_selection((struct rt_selection *)s);
	    bu_ptbl_rm(selections, s);
	}
	bu_ptbl_ins(selections, (long *)new_selection);
    } else if (BU_STR_EQUAL(cmd, "translate")) {
	struct rt_selection_operation operation;

	/*        4       5  6  7
	 * selection_name dx dy dz
	 */
	if (argc != 8) {
	    return GED_ERROR;
	}
	selection_name = argv[4];

	selection_set = ged_get_selection_set(gedp, solid_name, selection_name);
	selections = &selection_set->selections;

	if (BU_PTBL_LEN(selections) < 1) {
	    return GED_OK;
	}

	for (i = 0; i < (int)BU_PTBL_LEN(selections); ++i) {
	    int ret;
	    operation.type = RT_SELECTION_TRANSLATION;
	    operation.parameters.tran.dx = atof(argv[5]);
	    operation.parameters.tran.dy = atof(argv[6]);
	    operation.parameters.tran.dz = atof(argv[7]);

	    ret = ip->idb_meth->ft_process_selection(ip, gedp->dbip,
		    (struct rt_selection *)BU_PTBL_GET(selections, i), &operation);

	    if (ret != 0) {
		return GED_ERROR;
	    }
	}
    }

    return 0;
}

int
ged_joint2_core(struct ged *gedp, int argc, const char *argv[])
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

    if ((ndp = db_lookup(gedp->dbip,  argv[1], LOOKUP_NOISY)) == RT_DIR_NULL) {
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

    /* check for selection command */
    if (BU_STR_EQUAL(argv[2], "selection")) {
	int ret = joint_selection(gedp, &intern, argc, argv);
	if (BU_STR_EQUAL(argv[3], "translate") && ret == 0) {
	    GED_DB_PUT_INTERNAL(gedp, ndp, &intern, &rt_uniresource, GED_ERROR);
	}
	rt_db_free_internal(&intern);
	return ret;
    }

    rt_db_free_internal(&intern);

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl joint2_cmd_impl = {
    "joint2",
    ged_joint2_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd joint2_cmd = { &joint2_cmd_impl };
const struct ged_cmd *joint2_cmds[] = { &joint2_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  joint2_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
