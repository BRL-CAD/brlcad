/*                        I N S I D E . C
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
/** @file libged/inside.c
 *
 * The inside command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>


#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "rt/db4.h"

#include "../ged_private.h"

int
ged_inside_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *outdp;
    struct rt_db_internal intern;
    int arg = 1;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    RT_DB_INTERNAL_INIT(&intern);

    if (argc < arg+1) {
	bu_vls_printf(gedp->ged_result_str, "Enter name of outside solid: ");
	return BRLCAD_MORE;
    }
    if ((outdp = db_lookup(gedp->dbip,  argv[arg], LOOKUP_QUIET)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s not found", argv[0], argv[arg]);
	return BRLCAD_ERROR;
    }
    ++arg;

    if (rt_db_get_internal(&intern, outdp, gedp->dbip, bn_mat_identity, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	return BRLCAD_ERROR;
    }

    return ged_inside_internal(gedp, &intern, argc, argv, arg, outdp->d_namep);
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl inside_cmd_impl = {
    "inside",
    ged_inside_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd inside_cmd = { &inside_cmd_impl };
const struct ged_cmd *inside_cmds[] = { &inside_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  inside_cmds, 1 };

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
