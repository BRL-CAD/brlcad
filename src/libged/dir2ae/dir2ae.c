/*                         D I R 2 A E. C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/dir2ae.c
 *
 * The dir2ae command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/opt.h"

#include "../ged_private.h"


struct dir2ae_args {
    int inverse;
};

#define DIR2AE_OPTIONS(args) \
    BU_OPT_FLAG(args, "i", NULL, inverse, \
	"Interpret the inverse direction"),

BU_OPT_DESC_BUILDER(dir2ae_options, struct dir2ae_args, DIR2AE_OPTIONS);

static const ged_opt_spec dir2ae_opt_spec =
    GED_OPT("dir2ae",
	"Convert a direction vector to azimuth/elevation", dir2ae_options,
	"options-first direction:number{3}");


int
ged_dir2ae_core(struct ged *gedp, int argc, const char *argv[])
{
    fastf_t az, el;
    vect_t dir;
    double scan[3];
    struct dir2ae_args args = {0};
    int operand_count = 0;
    const char *command = argv[0];

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return GED_HELP;
    }

    argc--; argv++;
    operand_count = bu_opt_parse_build_with_policy(gedp->ged_result_str,
	argc, argv, dir2ae_options, &args, BU_OPT_PARSE_OPTIONS_FIRST);
    if (operand_count != 3) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[0], "%lf", &scan[X]) != 1 ||
	sscanf(argv[1], "%lf", &scan[Y]) != 1 ||
	sscanf(argv[2], "%lf", &scan[Z]) != 1) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return BRLCAD_ERROR;
    }
    /* convert from double to fastf_t */
    VMOVE(dir, scan);

	if (args.inverse)
	VSCALE(dir, dir, -1);

    bn_ae_vec(&az, &el, dir);
    bu_vls_printf(gedp->ged_result_str, "%lf %lf", az, el);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_DIR2AE_COMMANDS(X, XID) \
    X(dir2ae, ged_dir2ae_core, GED_CMD_DEFAULT, &dir2ae_opt_spec) \

GED_DECLARE_COMMAND_SET_WITH_OPT_SPEC(GED_DIR2AE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_OPT_SPEC("libged_dir2ae", 1, GED_DIR2AE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
