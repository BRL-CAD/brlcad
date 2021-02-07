/*                         G E T _ E Y E M O D E L . C
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
/** @file libged/get_eyemodel.c
 *
 * The get_eyemodel command.
 *
 */

#include "common.h"


#include "../ged_private.h"


/*
 * Returns the views eyemodel
 *
 * Usage:
 * get_eyemodel
 *
 */
int
ged_get_eyemodel_core(struct ged *gedp, int argc, const char *argv[])
{
    quat_t quat;
    vect_t eye_model;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return GED_ERROR;
    }

    _ged_rt_set_eye_model(gedp, eye_model);
    quat_mat2quat(quat, gedp->ged_gvp->gv_rotation);

    bu_vls_printf(gedp->ged_result_str, "viewsize %.15e;\n", gedp->ged_gvp->gv_size);
    bu_vls_printf(gedp->ged_result_str, "orientation %.15e %.15e %.15e %.15e;\n",
		  V4ARGS(quat));
    bu_vls_printf(gedp->ged_result_str, "eye_pt %.15e %.15e %.15e;\n",
		  eye_model[X], eye_model[Y], eye_model[Z]);

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl get_eyemodel_cmd_impl = {
    "get_eyemodel",
    ged_get_eyemodel_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd get_eyemodel_cmd = { &get_eyemodel_cmd_impl };
const struct ged_cmd *get_eyemodel_cmds[] = { &get_eyemodel_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  get_eyemodel_cmds, 1 };

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
