/*                         P U S H . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2020 United States Government as represented by
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
/** @file libged/push.c
 *
 * The push command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/cmd.h"
#include "bu/opt.h"

#include "../ged_private.h"

static void
npush_usage(struct bu_vls *str, struct bu_opt_desc *d) {
    char *option_help = bu_opt_describe(d, NULL);
    bu_vls_sprintf(str, "Usage: npush [options] obj\n");
    bu_vls_printf(str, "\nPushes position/rotation matrices 'down' the tree hierarchy, altering existing geometry as needed.  Default behavior clears all matrices from tree, unless push requires creation of new geometry objects.\n\n");
    if (option_help) {
	bu_vls_printf(str, "Options:\n%s\n", option_help);
	bu_free(option_help, "help str");
    }
}


extern "C" int
ged_npush_core(struct ged *gedp, int argc, const char *argv[])
{
    int print_help = 0;
    int xpush = 0;
    int to_regions = 0;
    int to_solids = 0;
    int max_depth = 0;
    struct bu_opt_desc d[8];
    BU_OPT(d[0], "h", "help",      "",   NULL,         &print_help,  "Print help and exit");
    BU_OPT(d[1], "?", "",          "",   NULL,         &print_help,  "");
    BU_OPT(d[2], "f", "force",     "",   NULL,         &xpush,       "Create new objects if needed to push matrices (xpush)");
    BU_OPT(d[3], "x", "xpush",     "",   NULL,         &xpush,       "");
    BU_OPT(d[4], "r", "regions",   "",   NULL,         &to_regions,  "Halt push at regions (matrix will be above region reference)");
    BU_OPT(d[5], "s", "solids",    "",   NULL,         &to_solids,   "Halt push at solids (matrix will be above solid reference)");
    BU_OPT(d[6], "d", "max-depth", "",   &bu_opt_int,  &max_depth,   "Halt at depth # from tree root (matrix will be above item # layers deep)");

    BU_OPT_NULL(d[7]);

    /* parse standard options */
    int opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (argc == 1 || print_help) {
	struct bu_vls npush_help = BU_VLS_INIT_ZERO;
	npush_usage(&npush_help, d);
	bu_vls_sprintf(gedp->ged_result_str, "%s", bu_vls_cstr(&npush_help));
	bu_vls_free(&npush_help);
	return GED_OK;
    }

    /* adjust argc to match the leftovers of the options parsing */
    argc = opt_ret;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    return GED_OK;
}

extern "C" {
#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl npush_cmd_impl = {
    "npush",
    ged_npush_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd npush_cmd = { &npush_cmd_impl };
const struct ged_cmd *npush_cmds[] = { &npush_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  npush_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */
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
