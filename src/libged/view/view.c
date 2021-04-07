/*                         V I E W . C
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
/** @file libged/view.c
 *
 * The view command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"
#include "./ged_view.h"


int
ged_view_func_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "quat|ypr|aet|center|eye|size [args]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (6 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "quat")) {
	return ged_quat_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "ypr")) {
	return ged_ypr_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "aet")) {
	return ged_aet_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "center")) {
	return ged_center_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "eye")) {
	return ged_eye_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "faceplate")) {
	return ged_faceplate_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "size")) {
	return ged_size_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "data_lines") || BU_STR_EQUAL(argv[1], "sdata_lines")) {
	return ged_view_data_lines(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "snap")) {
	return ged_view_snap(gedp, argc-1, argv+1);
    }

    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_ERROR;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"

struct ged_cmd_impl view_func_cmd_impl = {"view_func", ged_view_func_core, GED_CMD_DEFAULT};
const struct ged_cmd view_func_cmd = { &view_func_cmd_impl };

struct ged_cmd_impl view_cmd_impl = {"view", ged_view_func_core, GED_CMD_DEFAULT};
const struct ged_cmd view_cmd = { &view_cmd_impl };

struct ged_cmd_impl quat_cmd_impl = {"quat", ged_quat_core, GED_CMD_DEFAULT};
const struct ged_cmd quat_cmd = { &quat_cmd_impl };

struct ged_cmd_impl ypr_cmd_impl = {"ypr", ged_ypr_core, GED_CMD_DEFAULT};
const struct ged_cmd ypr_cmd = { &ypr_cmd_impl };

struct ged_cmd_impl aet_cmd_impl = {"aet", ged_aet_core, GED_CMD_DEFAULT};
const struct ged_cmd aet_cmd = { &aet_cmd_impl };

struct ged_cmd_impl ae_cmd_impl = {"ae", ged_aet_core, GED_CMD_DEFAULT};
const struct ged_cmd ae_cmd = { &ae_cmd_impl };

struct ged_cmd_impl center_cmd_impl = {"center", ged_center_core, GED_CMD_DEFAULT};
const struct ged_cmd center_cmd = { &center_cmd_impl };

struct ged_cmd_impl eye_cmd_impl = {"eye", ged_eye_core, GED_CMD_DEFAULT};
const struct ged_cmd eye_cmd = { &eye_cmd_impl };

struct ged_cmd_impl eye_pt_cmd_impl = {"eye_pt", ged_eye_core, GED_CMD_DEFAULT};
const struct ged_cmd eye_pt_cmd = { &eye_pt_cmd_impl };

struct ged_cmd_impl size_cmd_impl = {"size", ged_size_core, GED_CMD_DEFAULT};
const struct ged_cmd size_cmd = { &size_cmd_impl };

struct ged_cmd_impl data_lines_cmd_impl = {"data_lines", ged_view_data_lines, GED_CMD_DEFAULT};
const struct ged_cmd data_lines_cmd = { &data_lines_cmd_impl };

struct ged_cmd_impl sdata_lines_cmd_impl = {"sdata_lines", ged_view_data_lines, GED_CMD_DEFAULT};
const struct ged_cmd sdata_lines_cmd = { &sdata_lines_cmd_impl };

const struct ged_cmd *view_cmds[] = {
    &view_func_cmd,
    &view_cmd,
    &quat_cmd,
    &ypr_cmd,
    &aet_cmd,
    &ae_cmd,
    &center_cmd,
    &eye_cmd,
    &eye_pt_cmd,
    &size_cmd,
    &data_lines_cmd,
    &sdata_lines_cmd,
    NULL
};

static const struct ged_plugin pinfo = { GED_API,  view_cmds, 12 };

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
