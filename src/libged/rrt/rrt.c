/*                         R R T . C
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
/** @file libged/rrt.c
 *
 * The rrt command.
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bu/cmd.h"


#include "../ged_private.h"


int
ged_rrt_core(struct ged *gedp, int argc, const char *argv[])
{
    char **vp;
    int i;
    size_t args;
    char **gd_rt_cmd = NULL;
    int gd_rt_cmd_len = 0;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    args = argc + 2 + ged_who_argc(gedp);
    gd_rt_cmd = (char **)bu_calloc(args, sizeof(char *), "alloc gd_rt_cmd");

    vp = &gd_rt_cmd[0];
    for (i = 1; i < argc; i++)
	*vp++ = (char *)argv[i];
    *vp++ = gedp->dbip->dbi_filename;

    gd_rt_cmd_len = ged_who_argv(gedp, vp, (const char **)&gd_rt_cmd[args]);

    (void)_ged_run_rt(gedp, gd_rt_cmd_len, (const char **)gd_rt_cmd, -1, NULL);

    bu_free(gd_rt_cmd, "free gd_rt_cmd");

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl rrt_cmd_impl = {
    "rrt",
    ged_rrt_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd rrt_cmd = { &rrt_cmd_impl };
const struct ged_cmd *rrt_cmds[] = { &rrt_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  rrt_cmds, 1 };

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
