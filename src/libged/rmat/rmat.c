/*                         R M A T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file libged/rmat.c
 *
 * The rmat command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


int
ged_rmat_core(struct ged *gedp, int argc, const char *argv[])
{
    mat_t rotation;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* get the rotation matrix */
    if (argc == 1) {
	bn_encode_mat(gedp->ged_result_str, gedp->ged_gvp->gv_rotation, 1);
	return GED_OK;
    } else if (argc == 2) {
	/* set rotation matrix */
	if (bn_decode_mat(rotation, argv[1]) != 16)
	    return GED_ERROR;

	MAT_COPY(gedp->ged_gvp->gv_rotation, rotation);
	bv_update(gedp->ged_gvp);

	return GED_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
    return GED_ERROR;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl rmat_cmd_impl = {
    "rmat",
    ged_rmat_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd rmat_cmd = { &rmat_cmd_impl };
const struct ged_cmd *rmat_cmds[] = { &rmat_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  rmat_cmds, 1 };

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
